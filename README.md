# OpenRB PCF

A JUCE VST3 / AU / standalone effect that wraps the Pattern Controlled Filter from
[OpenRB-338](https://github.com/pingw33n/openrb-338). The DSP is a copy of `orb-engine`'s
`Pcf` in `third_party/openrb-338`; this project does not rewrite it.

The filter always runs at 44100 Hz in 64-sample blocks, the same grid the original uses. Host
audio at any other rate is resampled in and out. While the transport is moving, the pattern clock
advances like ReBirth's song position: one block of `tempo * TEMPO_STEP` per 64 samples. A host
position that moves with the audio locks that clock; a position that stays put does not restart
the pattern. Stopped holds the tick. A seek is picked up on the next block. 120 BPM is used when
the host has no tempo.

This plugin is GPL-3.0-only. JUCE is pulled under its AGPLv3. The engine copy is the
`orb-engine` and `rbs` crates only, under the same license. Copyright (C) 2026 the
OpenRB-338 authors. OpenRB-338 itself is not modified.

## Layout

```
third_party/openrb-338/crates/orb-engine   # copied filter engine
third_party/openrb-338/crates/rbs           # copied song-format crate the engine links
```

## Build

Rust 1.99.1 (see `rust-toolchain.toml`) and CMake 3.24+.

```sh
cargo test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

VST3, AU, and the standalone app are copied into `output/` after the build. Nothing is copied
into the system plug-in folders.

```
output/VST3/OpenRB PCF.vst3
output/AU/OpenRB PCF.component
output/Standalone/OpenRB PCF.app
```

## Controls

| Control    | Range        | Notes                          |
|------------|--------------|--------------------------------|
| Frequency  | 0..127       | cutoff base                    |
| Resonance  | 0..127       | one of 8 table rows            |
| Amount     | 0..127       | envelope height                |
| Decay      | 0..127       | envelope decay                 |
| Pattern    | 0..54        | 55 waves; lengths 2..32 steps  |
| Mode       | LP / BP      | engine mode `1` is low-pass    |

At 44100 Hz the resamplers are bypassed, so the samples that reach `Pcf` are the host samples.
