M8 optimization experiments — 2026-10-15

Fresh sequential NanoEmu runs using `run-in-emulator.sh unlocked`, 8-second warmup, full input exercise, 24 screenshots at 2-second intervals, audio capture and MBXR stats. Each trial starts from the current working sources, not the earlier measured binary (20.56 unlocked / 19.97 stock). Existing source edits are preserved. FPS is screenshot guest telemetry; host renderer FPS is not guest FPS. All trials use MAX; no physical device, SDK or reference reVC changes.

| Trial | Guest FPS average | Sample min–max | Draw µs | Audio µs | Draws | Result |
|---|---:|---:|---:|---:|---:|---|
| 01-max-baseline | 23.25 | 19.4–25.4 | 15723 | 290 | 14 | sustained_fps_24plus; image FAIL |
| 02-rgb565-rgba5551 | 21.73 | 14.9–24.0 | 18148 | 155 | 14 | sustained_fps_24plus; image FAIL |
| 03-far180 | 22.3 | 19.6–25.5 | 17332 | 570 | 14 | sustained_fps_24plus; image FAIL |
| 04-skip-translucent | Invalid | — | — | — | — | White/corrupt frames; telemetry rejected |
| 05-audio2048 | 20.87 | 18.6–23.4 | 19865 | 894 | 14 | sustained_fps_24plus; image FAIL |
| 06-mbxr-profile | 16.38 | 10.9–20.1 | 25989 | 759 | 15 | sustained_fps_24plus; image FAIL |
| 07-o3-lto | Invalid | — | — | — | — | White/corrupt frames; telemetry rejected |
| 08-translucent-retry | 18.49 | 11.3–21.9 | 22856 | 730 | 14 | sustained_fps_24plus; image FAIL |

Result: sustained 24+ FPS was not achieved. Valid guest averages range from 16.38 to 23.25 FPS, and every valid run has samples below 24 FPS. No experimental optimization is retained. This is emulator evidence only.

The unchanged baseline repeated at 16.38 FPS after initially reaching 23.25 FPS. Host scheduling, emulator timing and frame-dependent player movement are confounders; these single sequential runs do not establish causal speedups or regressions. There is no stable matched camera trace. Historical 20.56/19.97 FPS results used a different audio source snapshot, as described in [REPORT.md](REPORT.md), and are not direct controls for this working tree.

Experiment details, in requested order:

1. **MAX clocks confirmed.** Every run logs `[nano7-power] profile MAX: CPU 684 MHz, GPU 266 MHz`; run metadata records MAX. The existing runner sets `NANO7_CLOCK_PROFILE=MAX` and adds `0x08783d68 0x0000010a` to its per-run emulator poke file. These are emulated clocks, not verification of physical hardware clocks. Fresh MAX baseline: **23.25 FPS**, 19.4–25.4 across 24 samples.

2. **Direct RGB565/RGBA5551 instead of PAL8: 21.73 FPS.** Bypassed compressed upload, expanded the existing palette/index data to RGB565, and converted alpha texels from RGBA4444 to RGBA5551 (alpha threshold 8/15). Used the existing fallback allocation/upload path; its counters therefore count these intentional direct uploads as fallbacks. This isolates upload/storage format while retaining existing quantized colors, geometry and sampling; it is not a new full-quality asset conversion. Driver texture accounting remains **53,248 bytes** for 26 initial textures because PAL8 was already expanded to 16-bit texels. No resident-bandwidth reduction was demonstrated. One-bit alpha loses intermediate transparency. PVRTC was not tested: the requested native RGB565/RGBA5551 alternative was exercised, so this result says nothing about PVRTC performance.

3. **Far plane 380 → 180 units: 22.30 FPS.** Existing camera and material-sphere frustum tests use the shorter distance. Final world submission remained 14 draws, so this setting did not substantially reduce visible submission on the sampled route. It clips distant content and is not evidence that stronger geometry/object reduction could not help.

4. **Omit translucent batches:** first attempt produced white/corrupt frames after initially valid telemetry; no FPS can be inferred from the sample rows. The unchanged retry produced **18.49 FPS**, 11.3–21.9 across samples. The trial skips `batch.alpha` draws, rather than rendering transparent surfaces as opaque, and leaves batch preparation/barrier construction intact. It tests removal of translucent submissions and associated overdraw, not barrier-free batch rebuilding. Final submission was still 14 draws. Alpha testing is already disabled in `m8ResetRenderState`; there was no enabled alpha-test path to remove. No performance benefit was established, and removal of translucent content is a visual tradeoff. Failure of the first attempt cannot be attributed to a specific cause from these logs.

5. **Audio DMA buffers 8192 → 2048 frames, four buffers retained: 20.87 FPS.** Buffer duration falls from about 185.8 ms to 46.4 ms at 44.1 kHz, increasing refill frequency. Output sample rate, voice mixing, track and per-frame service calls remain unchanged. Sampled audio work increased from 290 to 894 µs/frame; capture reports active audio, nonzero peak and zero dropped bytes. This does not prove absence of audible underruns or glitches. The smaller buffer did not establish a gain. No sample-rate reduction or mixer algorithm rewrite was tested.

6. **`NANO7_MBXR_STATS` profiling: 16.38 FPS** on the unchanged baseline repeat. The existing checker enables this variable for every run, so this is a profiling repeat, not a stats-on/off comparison. The host metrics below average all reported windows, including launcher/warmup/input activity; they are not isolated gameplay GPU timings.

| Run | Host windows | Texture load/decode ms/job | Total render ms/job |
|---|---:|---:|---:|
| [01-max-baseline](optimization-runs/01-max-baseline/emulator.log) | 50 | 0.177 | 3.324 |
| [02-rgb565-rgba5551](optimization-runs/02-rgb565-rgba5551/emulator.log) | 48 | 0.175 | 3.375 |
| [03-far180](optimization-runs/03-far180/emulator.log) | 50 | 0.184 | 3.404 |
| [04-skip-translucent](optimization-runs/04-skip-translucent/emulator.log) | 45 | 0.085 | 5.081 |
| [05-audio2048](optimization-runs/05-audio2048/emulator.log) | 48 | 0.189 | 3.521 |
| [06-mbxr-profile](optimization-runs/06-mbxr-profile/emulator.log) | 42 | 0.210 | 3.876 |
| [07-o3-lto](optimization-runs/07-o3-lto/emulator.log) | 34 | 0.236 | 4.191 |
| [08-translucent-retry](optimization-runs/08-translucent-retry/emulator.log) | 44 | 0.236 | 4.355 |

`perf_load_tex()` in the existing emulator times `load_tex()`. Total render includes texture work, geometry/raster work and other render overhead; it is not a pure raster timer, and the remainder cannot be labeled exact raster time. On the profiling repeat, texture load/decode is about 5.4% of total host render time. These counters do not measure physical MBX bandwidth, per-fragment texture sampling cost, guest CPU time or presentation/scheduling delays. The data does not support texture loading as the dominant host render cost. White-frame runs' host counters are retained for diagnosis only.

7. **Compiler O3/LTO: build and launch succeeded, sampled FPS invalid.** The inherited Makefile already applies `-O3 -fomit-frame-pointer -ffast-math` to C++, and the existing librw toolchain also uses O3. Added `-flto` to the nine C++ object compilations and `-flto -O3` to the hbapp link. C/SDK compilation retained its inherited `-Os`; librw/newlib were existing non-LTO archives. This was mixed LTO across hbapp C++ units, not whole-program LTO or a rebuild of the external SDK. During sampling the display became white/corrupt, so apparent enormous decoded FPS values are rejected. This trial provides no trustworthy FPS comparison and no evidence of a working LTO optimization. See its saved Makefile and runner log for actual commands.

Validation and artifacts:

- All eight runs completed the 24-screenshot capture and passed source-disk size/mtime/replaced-block hash checks. None passed the runner's full screenshot check. In the valid runs, at least one image fell below its >1,000 colored-pixel threshold; baseline minimum was 902. The threshold was not relaxed. The first translucent and LTO attempts additionally had visibly white/corrupt frames and invalid bit-row telemetry.
- For valid runs 01, 02, 03, 05, 06 and 08, separately invoking the existing `verify.py` after the screenshot failure passed every capability check except sustained FPS: input/movement, camera, heap, audio active, guest errors, budget, advancing frames, disk integrity, changing scene and render-state indicators. This does not override the screenshot failures.
- The raw verifier outputs for invalid runs contain nonsensical decoded numbers; use the **Invalid** entries in this report, not those values, for conclusions. No sample-average ≥24 result is being accepted as sustained performance.
- Each linked run directory contains `runner.log`, `emulator.log`, `run-metadata.json`, `command.json`, `check.json`, `capabilities.json`, screenshots and audio. [profiling-summary.json](optimization-runs/profiling-summary.json) records clock evidence, host averages, I2S summaries and binary/emulator hashes. [source-before.json](optimization-runs/source-before.json) saves the starting versions of the four experiment source files; individual trial directories save source variants for review.

Runs used the existing `m8/run-in-emulator.sh unlocked` and `emulator_check.py`, with eight-second warmup, the full input exercise, 24 samples two seconds apart, audio capture and keep-awake. A temporary `M8_OUTPUT` override directed each run into `m8/optimization-runs/<trial>`; that runner edit was removed afterward. The performance verifier was invoked separately when the runner stopped at the image gate. To reproduce the output redirection, temporarily replace the runner's `OUTPUT="$HERE/pass"` assignment with `OUTPUT="${M8_OUTPUT:-$HERE/pass}"` and set `M8_OUTPUT` to the desired absolute directory. Apply the saved trial sources, force rebuild when changing compiler flags, and restore the starting source afterward.

All four experiment source files (model.cpp, nano.cpp, audio.cpp and Makefile) were checked byte-for-byte against their saved starting versions and restored. The original hbapp was rebuilt successfully after the last trial; existing unrelated edits and previous benchmark folders were preserved. No physical device, SDK source, reference reVC tree or emulator source was modified. Guest NAND writes remained in emulator memory/overlay files. Work stops with this report; further optimization or deployment is not performed.
