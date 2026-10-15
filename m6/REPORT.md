# M6 — audio

**PASS for bounded SFX and radio in the reduced reVC build in NanoEmu.** This follows the requested M6 audio scope; the older execution-plan table assigns audio to M9. The same ARM package renders M5's world through retained reVC fakerw/clump/texture helpers while game-facing `DMAudio` calls drive audio. No physical device, SDK or reference reVC tree was modified. No commit was made.

## Implementation

[`audio.cpp`](audio.cpp) provides eight mono SFX voices, per-voice volume/pan, signed 32-bit accumulation with s16 saturation, entity destruction/completion, master gains, radio start/stop and millisecond seek. The reduced `cDMAudio` contract retains 13 original method signatures and the verbatim `eAudioType` enum; [source verification](source-check.json) checks these against the read-only reference. It replaces the audio adapter for this selected build; it does not link the original AudioManager or all full-game DMAudio methods. Reduced sound IDs 0–2 select three real VC effects; station 0 selects the supplied radio excerpt.

[`prepare-audio.py`](prepare-audio.py) converts AirHornL, BikeRev and CameraL to 8 kHz mono s16 PCM, 6,400 bytes each. PCM needs no runtime codec. A 60-second FEVER excerpt is converted offline to 8 kHz mono IMA ADPCM: 247,500 compressed bytes, with independent 256-sample blocks. Runtime reads one 132-byte block into a small scratch array and decodes into a reusable 512-byte cache. Rational phase accumulators resample both paths to 44,100 Hz interleaved stereo. This is deliberately reduced audio quality; no floating-point decoder, MP3 library, OpenAL, thread or full-track PCM allocation is required. [Provenance](asset-provenance.json) records source hashes. Generated user-owned audio is ignored.

Radio streams from embedded immutable compressed backing storage, with bounded reads/decoding. The compressed minute is resident in the executable; this does not prove NAND/file streaming or a complete radio station archive. The private IMA block layout is documented in [README](README.md).

The snapshot-specific output adapter saves DMA engine 1/channel 2 and the I2S divisor, stops the OS transfer and installs four cache-cleaned 8,192-frame buffers in a linked DMA ring. It uses the existing I2S0 sink and RetailOS IRQ handler. `Service` refills completed buffers. `Terminate` stops DMA, restores the saved OS channel/divisor, frees the sole audio allocation, verifies the OS heap recovers at least its requested size, and clears the tracked live balance. No emulator patch or host audio injection is used. Four buffers provide 743 ms of queueing; low latency and recovery from a stalled service loop remain future work. The current SDK GL contract has no quit callback: a production integration must call `Terminate` before unloading the app.

## Verification

[Host checks](host-check.log) pass with ASan/UBSan: eight simultaneous voices, entity teardown, silence after stop, deterministic seek/replay, EOF, invalid station, idempotent terminate and 1,000 lifecycle cycles. LeakSanitizer is disabled because the environment runs under ptrace; the host mixer makes no heap allocations. [Package checks](package-check.log) pass HRL1 bounds, 1,331 relocations and nine relative constructors. The ARM ELF has [no undefined symbols](undefined-symbols.log); the SDK Thumb/soft-float gate passes. The package is 990,436 bytes, with a 985,088-byte image, 1,032,096-byte span and 47,008 BSS bytes, fitting the existing executable slot.

[QMP verification](run/capabilities.json) passes all 16 advancing screenshots, frames 1122–1482, with live scene rendering and no GL, memory or adapter errors. A representative view is [the final frame](run/vcaudio-15.png). The existing runner uses read-only NAND with an in-memory executable overlay. [Disk integrity](run/check.json) confirms source size, mtime and every replaced-block hash are unchanged.

The existing emulator I2S sink captured **24,399,872 bytes (138.32 seconds)** of 44.1 kHz s16 stereo, available locally as `run/audio.wav`. Verification finds an exact 32,768-byte block containing radio plus three simultaneous effects at PCM byte offset 2,805,760. It also finds exact reference fingerprints from **all 60 consecutive radio seconds**, in order and with one constant capture/source alignment (701,440 frames). This proves the radio advances through the minute, rather than repeating a short buffer or merely capturing unrelated OS audio. The guest reports one radio EOF and a peak of **6 simultaneous SFX voices**; eight-voice capacity is separately exercised by the host check. Output saturates safely at signed-16 limits.

| Measurement | NanoEmu result |
|---|---:|
| Full audio stop/reinitialise cycles | 28 |
| Audio live / peak heap allocation | 131,184 / 131,184 B |
| Warm OS free heap, world fully unloaded and audio active | 6,349,592–6,349,692 B |
| Comparable unloaded-world heap range | 100 B |
| Mean decode/resample/mix time per 8,192 frames | 4.641 ms |
| Maximum block mix time | 18.900 ms |
| Available audio time per block | 185.760 ms |
| Mean / maximum measured mix share | 2.50% / 10.17% |

Audio tracked live bytes return to zero inside each `Terminate`, before reinitialisation. Each guest free checks immediate heap recovery; all checks report zero errors. Repeated warmed full-world unloads provide a comparable OS heap state across cycles, with only 100 bytes of variation. This establishes bounded audio memory in this reduced workload and remains well below M5's 18 MiB policy envelope; it is not a full-game shared-GPU budget proof.

Timing uses the guest microsecond counter around decoding/resampling/mixing, excluding cache cleaning and DMA submission. The maximum includes scheduling interference. **NanoEmu is not cycle-accurate: these measurements do not establish CPU cost at 342 MHz on hardware.** The existing Cortex-A5 Thumb soft-float build flags are retained. Device timing, low-latency scheduling and actual codec output remain separate gates.

Package SHA256: `789eb9408b200112041441827018d2bff4f8e477b64d1661a30cb8ca97c7ccc7`. Capture SHA256: `1b099c204cdff7a1850c9b83b12b97b68d792b740f8476332d4f57270b462b98`. The ignored capture can be regenerated with the runner. See [README](README.md) for the exact reproduction steps and telemetry schema.
