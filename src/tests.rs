use super::*;
use orb_engine::pcf::{self, Pcf, WAVE};

fn ctx<'a>(tables: &'a Tables, tick: i32, tempo: u32) -> BlockContext<'a> {
    BlockContext {
        tables,
        meter_index: 0,
        tempo,
        tick,
        stack_word: None,
    }
}

fn apply(p: &mut Pcf, params: Params) {
    p.set_param(pcf::FREQUENCY as u8, params.frequency);
    p.set_param(pcf::RESONANCE as u8, params.resonance);
    p.set_param(pcf::AMOUNT as u8, params.amount);
    p.set_param(pcf::DECAY as u8, params.decay);
    p.set_param(pcf::WAVE as u8, params.pattern.min(PATTERN_MAX));
    p.set_param(pcf::MODE as u8, params.mode);
    p.set_param(pcf::ENABLED as u8, 1);
    p.set_pattern_override(seq_override(&params));
}

fn seq_params(length: u8, resolution: u8, steps: [u8; 32]) -> Params {
    Params {
        seq_enable: 1,
        seq_length: length,
        seq_resolution: resolution,
        seq_steps: steps,
        amount: 40,
        frequency: 0,
        pattern: 0,
        ..Params::default()
    }
}

fn playing(ppq: f64) -> Playhead {
    Playhead {
        bpm: 120.0,
        ppq,
        playing: true,
    }
}

/// A few blocks through the wrapper equal `Pcf::process` on each channel, delayed by 64 samples.
#[test]
fn wrapper_matches_pcf_process() {
    let params = Params {
        pattern: 0,
        ..Params::default()
    };
    let nblocks = 4;
    let n = nblocks * BLOCK;
    let mut input_l = vec![0.0f32; n];
    let mut input_r = vec![0.0f32; n];
    for i in 0..n {
        input_l[i] = ((i % 17) as f32) * 0.02 - 0.1;
        input_r[i] = ((i % 13) as f32) * -0.015 + 0.05;
    }

    let mut wrap_l = input_l.clone();
    let mut wrap_r = input_r.clone();
    let mut wrap = PcfStereo::new();
    wrap.process(&mut wrap_l, &mut wrap_r, params, playing(0.0));

    assert!(wrap_l[..BLOCK].iter().all(|&x| x == 0.0));
    assert!(wrap_r[..BLOCK].iter().all(|&x| x == 0.0));

    let tables = Tables::new();
    let tempo = DEFAULT_TEMPO_MILLI;
    let mut left = Pcf::new();
    let mut right = Pcf::new();
    apply(&mut left, params);
    apply(&mut right, params);

    for k in 0..nblocks - 1 {
        let mut bl: Block = input_l[k * BLOCK..(k + 1) * BLOCK].try_into().unwrap();
        let mut br: Block = input_r[k * BLOCK..(k + 1) * BLOCK].try_into().unwrap();
        let tick = block_tick(0.0, 120.0, k);
        left.process(&mut bl, &ctx(&tables, tick, tempo));
        right.process(&mut br, &ctx(&tables, tick, tempo));
        assert_eq!(
            wrap_l[(k + 1) * BLOCK..(k + 2) * BLOCK],
            bl,
            "left block {k}"
        );
        assert_eq!(
            wrap_r[(k + 1) * BLOCK..(k + 2) * BLOCK],
            br,
            "right block {k}"
        );
    }
}

/// Consecutive blocks at 120 BPM use `trunc((k+1) * tempo * TEMPO_STEP)`.
#[test]
fn tick_sequence_at_120_bpm() {
    let tempo = DEFAULT_TEMPO_MILLI;
    let inc = tempo as f64 * TEMPO_STEP;
    assert_eq!(TEMPO_STEP, 4.6439909297052154e-06);

    let params = Params {
        pattern: 0,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    let ppq_per_block = BLOCK as f64 / ENGINE_RATE * 120.0 / 60.0;

    for k in 0..8 {
        let mut l = [0.1f32; BLOCK];
        let mut r = [0.1f32; BLOCK];
        p.process(&mut l, &mut r, params, playing(k as f64 * ppq_per_block));
        let want = seq::trunc((k as f64 + 1.0) * inc);
        assert_eq!(p.last_tick(), Some(want), "block {k}");
        assert_eq!(block_tick(k as f64 * ppq_per_block, 120.0, 0), want);
    }
}

/// A short buffer leaves a remainder; the next call finishes the block.
#[test]
fn short_buffer_leaves_a_remainder() {
    let params = Params::default();
    let mut p = PcfStereo::new();
    let mut l = [0.2f32; 32];
    let mut r = [0.2f32; 32];
    p.process(&mut l, &mut r, params, playing(0.0));
    assert_eq!(p.blocks_processed(), 0);
    assert_eq!(p.fill(), 32);
    assert_eq!(p.last_tick(), None);

    let mut l = [0.2f32; 32];
    let mut r = [0.2f32; 32];
    p.process(&mut l, &mut r, params, playing(0.0));
    assert_eq!(p.blocks_processed(), 1);
    assert_eq!(p.fill(), 0);
    assert_eq!(p.last_tick(), Some(block_tick(0.0, 120.0, 0)));
}

/// A seek from mid-pattern to the next bar start lands on step 0 for a 16-step pattern and opens the gate.
#[test]
fn seek_to_bar_start_opens_the_gate() {
    let params = Params {
        pattern: 0,
        amount: 40,
        frequency: 0,
        ..Params::default()
    };
    assert_eq!(pcf::pattern(0), (16, 16));
    assert_eq!(pcf::velocity(0, 0), 120);
    assert_eq!(pcf::velocity(0, 8), 0);

    let inc = DEFAULT_TEMPO_MILLI as f64 * TEMPO_STEP;
    // step 8 at 16ths: tick/48 % 16 = 8 → tick = 384
    let mid_tick = 384.0;
    let mid_ppq = (mid_tick - inc) / PPQ as f64;

    let mut p = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(mid_ppq));
    assert_eq!(p.left().step, 8);
    let mid_env = p.left().env;

    let bar_ppq = 4.0;
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(bar_ppq));
    assert_eq!(p.left().step, 0);
    assert!(
        p.left().env > mid_env + 1.0,
        "env after seek {} should open past mid {mid_env}",
        p.left().env
    );
}

/// A seek from step 0 to another bar's step 0 does not retrigger.
#[test]
fn seek_step0_to_step0_does_not_retrigger() {
    let params = Params {
        pattern: 0,
        amount: 40,
        frequency: 0,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(0.0));
    assert_eq!(p.left().step, 0);
    let env_after_first = p.left().env;
    assert!(env_after_first > 1.0, "step 0 of wave 0 opens the gate");

    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(4.0));
    assert_eq!(p.left().step, 0);
    assert!(
        p.left().env < env_after_first,
        "same step index must keep decaying, not retrigger: {} vs {env_after_first}",
        p.left().env
    );
}

#[test]
fn stopped_holds_the_tick() {
    let params = Params {
        pattern: 0,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    let mut l = [0.1f32; BLOCK];
    let mut r = [0.1f32; BLOCK];
    p.process(
        &mut l,
        &mut r,
        params,
        Playhead {
            bpm: 120.0,
            ppq: 0.0,
            playing: true,
        },
    );
    let tick = p.last_tick().unwrap();

    let mut l = [0.1f32; BLOCK];
    let mut r = [0.1f32; BLOCK];
    p.process(
        &mut l,
        &mut r,
        params,
        Playhead {
            bpm: 140.0,
            ppq: 8.0,
            playing: false,
        },
    );
    assert_eq!(p.last_tick(), Some(tick));
    assert_eq!(p.left().step, pcf::quotient(tick, 16).unwrap() % 16);
}

/// Stopped callbacks take step 0; Play with a stalled host position must still leave it.
#[test]
fn play_after_holds_with_stalled_ppq_starts_the_clock() {
    let params = Params {
        pattern: 0,
        amount: 40,
        frequency: 0,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    let stopped = Playhead {
        bpm: 120.0,
        ppq: 0.0,
        playing: false,
    };
    for _ in 0..8 {
        let mut l = [0.1f32; BLOCK];
        let mut r = [0.1f32; BLOCK];
        p.process(&mut l, &mut r, params, stopped);
    }
    assert_eq!(p.left().step, 0);

    p.restart_clock();
    for i in 0..100 {
        let mut l = [0.1f32; BLOCK];
        let mut r = [0.1f32; BLOCK];
        p.process(&mut l, &mut r, params, playing(0.0));
        if i == 0 {
            assert_eq!(p.left().step, 0);
            assert!(p.left().env > 1.0, "restarted downbeat must open the gate");
        }
    }
    assert!(
        p.last_tick().unwrap() >= 48,
        "tick {} never reached the next 16th",
        p.last_tick().unwrap()
    );
    assert_ne!(p.left().step, 0);
}

/// ReBirth keeps counting while playing. A host that repeats one quarter-note position
/// must not pin the pattern to step 0.
#[test]
fn stalled_playhead_keeps_rebirth_time() {
    let params = Params {
        pattern: 0,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    let mut prev = 0;
    for i in 0..100 {
        let mut l = [0.0f32; BLOCK];
        let mut r = [0.0f32; BLOCK];
        p.process(&mut l, &mut r, params, playing(0.0));
        let tick = p.last_tick().unwrap();
        if i > 0 {
            assert!(tick >= prev, "tick went backwards at block {i}");
        }
        prev = tick;
    }
    assert!(prev >= 48, "tick {prev} never reached the next 16th");
    assert_ne!(p.left().step, 0);
}

/// A stalled clock snaps back onto the host when the host position actually moves.
#[test]
fn stalled_clock_snaps_to_a_moving_playhead() {
    let params = Params {
        pattern: 0,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    for _ in 0..100 {
        let mut l = [0.0f32; BLOCK];
        let mut r = [0.0f32; BLOCK];
        p.process(&mut l, &mut r, params, playing(0.0));
    }
    assert_ne!(p.left().step, 0);

    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(4.0));
    assert_eq!(p.last_tick(), Some(block_tick(4.0, 120.0, 0)));
    assert_eq!(p.left().step, 0);
}

#[test]
fn pattern_is_clamped_to_the_panel_range() {
    let params = Params {
        pattern: 200,
        ..Params::default()
    };
    let mut p = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(0.0));
    assert_eq!(p.left().live[WAVE], PATTERN_MAX);
}

#[test]
fn seq_on_ignores_the_factory_wave() {
    let mut steps = [0u8; 32];
    steps[1] = 120;
    let params = seq_params(16, 16, steps);
    let mut p = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(0.0));
    assert_eq!(p.left().step, 0);
    assert!(
        p.left().env < 1.0,
        "custom rest at step 0 must not open, env {}",
        p.left().env
    );

    let inc = DEFAULT_TEMPO_MILLI as f64 * TEMPO_STEP;
    let step1_ppq = (48.0 - inc) / PPQ as f64;
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(step1_ppq));
    assert_eq!(p.left().step, 1);
    assert!(
        p.left().env > 1.0,
        "custom hit at step 1 must open, env {}",
        p.left().env
    );
}

#[test]
fn seq_off_keeps_the_factory_wave() {
    let mut steps = [0u8; 32];
    steps[0] = 120;
    let mut params = seq_params(16, 16, steps);
    params.seq_enable = 0;
    let mut p = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(0.0));
    assert_eq!(p.left().step, 0);
    assert!(
        p.left().env > 1.0,
        "wave 0 step 0 opens when seq is off, env {}",
        p.left().env
    );
}

#[test]
fn seq_resolution_changes_step_rate() {
    let mut steps = [0u8; 32];
    steps[1] = 120;
    let inc = DEFAULT_TEMPO_MILLI as f64 * TEMPO_STEP;
    // tick 24 is step 1 at 32nds (tick/24) and still step 0 at 16ths (tick/48).
    let ppq = (24.0 - inc) / PPQ as f64;

    let mut p32 = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p32.process(&mut l, &mut r, seq_params(16, 32, steps), playing(ppq));
    assert_eq!(p32.left().step, 1);

    let mut p16 = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p16.process(&mut l, &mut r, seq_params(16, 16, steps), playing(ppq));
    assert_eq!(p16.left().step, 0);
}

#[test]
fn seq_length_one_retriggers() {
    let mut steps = [0u8; 32];
    steps[0] = 120;
    let params = seq_params(1, 16, steps);
    let mut p = PcfStereo::new();
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(0.0));
    let first = p.left().env;
    assert!(first > 1.0);

    let inc = DEFAULT_TEMPO_MILLI as f64 * TEMPO_STEP;
    let next_ppq = (48.0 - inc) / PPQ as f64;
    let mut l = [0.0f32; BLOCK];
    let mut r = [0.0f32; BLOCK];
    p.process(&mut l, &mut r, params, playing(next_ppq));
    assert!(
        p.left().env > first * 0.5,
        "length 1 must retrigger on the next 16th, {} vs first {first}",
        p.left().env
    );
}
