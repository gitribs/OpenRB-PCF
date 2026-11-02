//! Stereo wrapper around [`orb_engine::pcf::Pcf`].
//!
//! Two PCF instances share one [`Tables`] and one song clock. Audio is processed in 64-sample
//! blocks at 44100 Hz, matching the original insert. Output is delayed by one block so a host
//! sees a constant 64-sample latency. The C API takes buffers already at 44100 Hz; resampling
//! is the JUCE plugin's job.
//!
//! The clock is ReBirth's song position. While playing, every completed block adds
//! `tempo * TEMPO_STEP`, the same step as the original sequencer. A host position that moves
//! with the audio replaces that position (start, seek, loop). A position that stays put does
//! not restart the pattern. Stopped holds the tick.

use orb_engine::frame::{BLOCK, Block, BlockContext, InsertEffect, SongDevice};
use orb_engine::pcf::{self, Pcf};
use orb_engine::seq::{self, TEMPO_STEP};
use orb_engine::song::PPQ;
use orb_engine::tables::Tables;

/// Engine (and original) sample rate. The coefficient tables, glide and decay are all in samples.
pub const ENGINE_RATE: f64 = 44100.0;
/// The last valid pattern index. Waves 0..54 are the 55 panel patterns.
pub const PATTERN_MAX: u8 = 54;
/// Fallback tempo when the host has none, as BPM*1000.
pub const DEFAULT_TEMPO_MILLI: u32 = 120_000;

/// Panel parameters applied at the start of each 64-sample block.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub struct Params {
    pub frequency: u8,
    pub resonance: u8,
    pub amount: u8,
    pub decay: u8,
    pub pattern: u8,
    /// `1` is low-pass; anything else is band-pass.
    pub mode: u8,
    pub bypass: u8,
    /// Non-zero: the custom step table drives the filter instead of [`Self::pattern`].
    pub seq_enable: u8,
    /// Loop length 1..=32 when the sequencer is on.
    pub seq_length: u8,
    /// Steps per bar: 8, 16 or 32. Any other value becomes 16.
    pub seq_resolution: u8,
    /// Engine velocities for the 32 pad slots (0 / 60 / 120 from the UI).
    pub seq_steps: [u8; 32],
}

impl Default for Params {
    fn default() -> Self {
        let d = pcf::DEFAULTS;
        Self {
            frequency: d[pcf::FREQUENCY],
            resonance: d[pcf::RESONANCE],
            amount: d[pcf::AMOUNT],
            decay: d[pcf::DECAY],
            pattern: d[pcf::WAVE],
            mode: d[pcf::MODE],
            bypass: 0,
            seq_enable: 0,
            seq_length: 16,
            seq_resolution: 16,
            seq_steps: [0; 32],
        }
    }
}

/// Host playhead at the first 44100 Hz frame of this callback.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Playhead {
    pub bpm: f64,
    pub ppq: f64,
    pub playing: bool,
}

/// C layout of [`Playhead`]: `playing` is a 32-bit flag, not a Rust `bool`.
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PlayheadC {
    pub bpm: f64,
    pub ppq: f64,
    pub playing: i32,
}

impl From<PlayheadC> for Playhead {
    fn from(p: PlayheadC) -> Self {
        Self {
            bpm: p.bpm,
            ppq: p.ppq,
            playing: p.playing != 0,
        }
    }
}

impl Default for Playhead {
    fn default() -> Self {
        Self {
            bpm: 120.0,
            ppq: 0.0,
            playing: false,
        }
    }
}

/// Song position, in ticks, at the end of block `k`. `ppq` is the first frame of the callback.
fn host_pos(ppq: f64, bpm: f64, k: usize) -> f64 {
    ppq * PPQ as f64 + (k as f64 + 1.0) * tempo_milli(bpm) as f64 * TEMPO_STEP
}

/// Tick of block `k` in a callback whose first 44100 Hz frame is at `ppq`.
pub fn block_tick(ppq: f64, bpm: f64, k: usize) -> i32 {
    seq::trunc(host_pos(ppq, bpm, k))
}

fn block_inc(bpm: f64) -> f64 {
    tempo_milli(bpm) as f64 * TEMPO_STEP
}

/// Quarter notes spanned by `n` frames at 44100 Hz.
fn quarters_in(n: usize, bpm: f64) -> f64 {
    let bpm = if !bpm.is_finite() || bpm <= 0.0 {
        120.0
    } else {
        bpm
    };
    n as f64 / ENGINE_RATE * bpm / 60.0
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum ClockMode {
    /// Transport stopped. The tick stays where it was.
    Hold,
    /// The host quarter-note position moved with the audio, or playback just started.
    Follow,
    /// The host position did not move. Keep counting the way the original song clock does.
    FreeRun,
}

fn tempo_milli(bpm: f64) -> u32 {
    if !bpm.is_finite() || bpm <= 0.0 {
        DEFAULT_TEMPO_MILLI
    } else {
        (bpm * 1000.0).round() as u32
    }
}

/// Stereo PCF: one clock, two filter memories, a 64-sample delay.
pub struct PcfStereo {
    tables: Tables,
    left: Pcf,
    right: Pcf,
    in_l: Block,
    in_r: Block,
    delay_l: Block,
    delay_r: Block,
    fill: usize,
    /// ReBirth song position in ticks, after the last completed block.
    pos: f64,
    was_playing: bool,
    last_ppq: Option<f64>,
    held_tick: i32,
    last_tick: Option<i32>,
    blocks_processed: u64,
}

impl Default for PcfStereo {
    fn default() -> Self {
        Self::new()
    }
}

impl PcfStereo {
    pub fn new() -> Self {
        Self {
            tables: Tables::new(),
            left: Pcf::new(),
            right: Pcf::new(),
            in_l: [0.0; BLOCK],
            in_r: [0.0; BLOCK],
            delay_l: [0.0; BLOCK],
            delay_r: [0.0; BLOCK],
            fill: 0,
            pos: 0.0,
            was_playing: false,
            last_ppq: None,
            held_tick: 0,
            last_tick: None,
            blocks_processed: 0,
        }
    }

    /// Clears filter memory, the delay and the clock. Tables stay.
    pub fn reset(&mut self) {
        self.left = Pcf::new();
        self.right = Pcf::new();
        self.in_l = [0.0; BLOCK];
        self.in_r = [0.0; BLOCK];
        self.delay_l = [0.0; BLOCK];
        self.delay_r = [0.0; BLOCK];
        self.fill = 0;
        self.pos = 0.0;
        self.was_playing = false;
        self.last_ppq = None;
        self.held_tick = 0;
        self.last_tick = None;
        self.blocks_processed = 0;
    }

    pub fn latency() -> usize {
        BLOCK
    }

    pub fn last_tick(&self) -> Option<i32> {
        self.last_tick
    }

    pub fn blocks_processed(&self) -> u64 {
        self.blocks_processed
    }

    pub fn process(
        &mut self,
        left: &mut [f32],
        right: &mut [f32],
        params: Params,
        playhead: Playhead,
    ) {
        let n = left.len().min(right.len());
        let tempo = tempo_milli(playhead.bpm);
        let mode = self.begin_clock(playhead, n);
        let mut i = 0;
        let mut k = 0usize;

        while i < n {
            let take = (n - i).min(BLOCK - self.fill);
            self.in_l[self.fill..self.fill + take].copy_from_slice(&left[i..i + take]);
            self.in_r[self.fill..self.fill + take].copy_from_slice(&right[i..i + take]);
            left[i..i + take].copy_from_slice(&self.delay_l[self.fill..self.fill + take]);
            right[i..i + take].copy_from_slice(&self.delay_r[self.fill..self.fill + take]);
            self.fill += take;
            i += take;

            if self.fill == BLOCK {
                let tick = self.take_tick(playhead, k, mode);
                if params.bypass == 0 {
                    self.apply_params(params);
                    let ctx = BlockContext {
                        tables: &self.tables,
                        meter_index: 0,
                        tempo,
                        tick,
                        stack_word: None,
                    };
                    self.left.process(&mut self.in_l, &ctx);
                    self.right.process(&mut self.in_r, &ctx);
                }
                if mode != ClockMode::Hold {
                    self.held_tick = tick;
                }
                self.last_tick = Some(tick);
                std::mem::swap(&mut self.in_l, &mut self.delay_l);
                std::mem::swap(&mut self.in_r, &mut self.delay_r);
                self.fill = 0;
                self.blocks_processed += 1;
                k += 1;
            }
        }
    }

    /// How this callback moves the song position. Decided once, then applied to every block.
    fn begin_clock(&mut self, playhead: Playhead, n: usize) -> ClockMode {
        if n == 0 || !playhead.playing {
            self.was_playing = false;
            return ClockMode::Hold;
        }
        let mode = if !self.was_playing {
            ClockMode::Follow
        } else if let Some(prev) = self.last_ppq {
            let expected = quarters_in(n, playhead.bpm);
            let delta = (playhead.ppq - prev).abs();
            if expected > 0.0 && delta < expected * 0.5 {
                ClockMode::FreeRun
            } else {
                ClockMode::Follow
            }
        } else {
            ClockMode::Follow
        };
        self.was_playing = true;
        self.last_ppq = Some(playhead.ppq);
        mode
    }

    fn take_tick(&mut self, playhead: Playhead, k: usize, mode: ClockMode) -> i32 {
        match mode {
            ClockMode::Hold => self.held_tick,
            ClockMode::Follow => {
                self.pos = host_pos(playhead.ppq, playhead.bpm, k);
                seq::trunc(self.pos)
            }
            ClockMode::FreeRun => {
                self.pos += block_inc(playhead.bpm);
                seq::trunc(self.pos)
            }
        }
    }

    fn apply_params(&mut self, p: Params) {
        let pattern = p.pattern.min(PATTERN_MAX);
        let frequency = p.frequency.min(127);
        let resonance = p.resonance.min(127);
        let amount = p.amount.min(127);
        let decay = p.decay.min(127);
        let seq = seq_override(&p);
        for inst in [&mut self.left, &mut self.right] {
            inst.set_param(pcf::FREQUENCY as u8, frequency);
            inst.set_param(pcf::RESONANCE as u8, resonance);
            inst.set_param(pcf::AMOUNT as u8, amount);
            inst.set_param(pcf::DECAY as u8, decay);
            inst.set_param(pcf::WAVE as u8, pattern);
            inst.set_param(pcf::MODE as u8, p.mode);
            inst.set_param(pcf::ENABLED as u8, 1);
            inst.set_pattern_override(seq);
        }
    }
}

fn seq_override(p: &Params) -> Option<pcf::PatternOverride> {
    if p.seq_enable == 0 {
        return None;
    }
    Some(pcf::PatternOverride {
        length: p.seq_length.clamp(1, 32) as u16,
        resolution: match p.seq_resolution {
            8 | 32 => p.seq_resolution as u16,
            _ => 16,
        },
        steps: p.seq_steps,
    })
}

#[cfg(test)]
impl PcfStereo {
    pub fn left(&self) -> &Pcf {
        &self.left
    }

    pub fn fill(&self) -> usize {
        self.fill
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn orb_pcf_new() -> *mut PcfStereo {
    Box::into_raw(Box::new(PcfStereo::new()))
}

/// # Safety
/// `pcf` is a pointer from [`orb_pcf_new`], or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn orb_pcf_free(pcf: *mut PcfStereo) {
    if !pcf.is_null() {
        drop(unsafe { Box::from_raw(pcf) });
    }
}

/// # Safety
/// `pcf` is a pointer from [`orb_pcf_new`], or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn orb_pcf_reset(pcf: *mut PcfStereo) {
    if let Some(p) = unsafe { pcf.as_mut() } {
        p.reset();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn orb_pcf_latency() -> i32 {
    PcfStereo::latency() as i32
}

/// # Safety
/// `pcf` is a pointer from [`orb_pcf_new`]. `left` and `right` each have `nframes` samples when
/// `nframes > 0`. `params` and `playhead` may be null, in which case defaults are used.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn orb_pcf_process(
    pcf: *mut PcfStereo,
    left: *mut f32,
    right: *mut f32,
    nframes: i32,
    params: *const Params,
    playhead: *const PlayheadC,
) {
    let Some(p) = (unsafe { pcf.as_mut() }) else {
        return;
    };
    if nframes <= 0 || left.is_null() || right.is_null() {
        return;
    }
    let n = nframes as usize;
    let left = unsafe { std::slice::from_raw_parts_mut(left, n) };
    let right = unsafe { std::slice::from_raw_parts_mut(right, n) };
    let params = unsafe { params.as_ref() }.copied().unwrap_or_default();
    let playhead = unsafe { playhead.as_ref() }
        .copied()
        .map(Playhead::from)
        .unwrap_or_default();
    p.process(left, right, params, playhead);
}

#[cfg(test)]
mod tests;
