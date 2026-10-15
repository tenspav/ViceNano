M6 adds bounded audio to the existing M5 reduced reVC world viewer. Run from this directory:

```
python3 prepare-audio.py
./check-host.sh
python3 check-interface.py
./run-in-emulator.sh
python3 verify.py
```

M5's generated world assets and ARM librw archive must already exist (`../m5/prepare-assets.py` and `make -C ../m5`). The audio packer requires ffmpeg and reads the local Vice City `Audio` directory without writing to it. Generated game audio, executable packages and captures are ignored. Python verification requires Pillow. The host check uses ASan/UBSan; LeakSanitizer is disabled because it cannot run under this environment's ptrace supervision. The mixer itself allocates no heap on the host.

`audio.cpp` implements a reduced `cDMAudio` contract with the original retained method names and argument types, including `eAudioType`. It implements eight mono voices, gain/pan, completion, entity destruction, effects/music master volume, track start/stop and millisecond radio seek. Reduced sound IDs 0/1/2 map to AirHornL/BikeRev/CameraL; station 0 maps to a 60-second excerpt from FEVER. Unpacked full-game event IDs, entity spatialisation, police speech and the full AudioManager are outside this reduced selection. Unsupported sounds/stations remain silent.

SFX are offline-converted 8 kHz signed PCM. Radio uses independent 256-sample IMA ADPCM blocks: 4-byte predictor/index header and 128 nibble bytes, decoded into one reusable 512-byte cache. This block container includes the predictor as a decoded nibble and is private to this packer/backend, not a WAV IMA container. Read-at currently copies 132 bytes from embedded immutable storage; compressed backing storage is resident in the executable, decoded track storage is bounded. Resampling to 44.1 kHz is an exact rational phase accumulator with nearest-neighbour samples. Quality is intentionally reduced for this milestone.

The NanoEmu snapshot-specific output adapter saves RetailOS DMA engine 1/channel 2, stops it, installs a four-descriptor ring and sets 44.1 kHz. It uses the existing I2S FIFO and RetailOS completion IRQ handler. `Service` refills completed 8,192-frame buffers, with SDK SVC cache cleaning before DMA use. `Terminate` stops DMA, restores the original channel/divisor, frees the sole audio allocation and checks that the OS heap recovers its allocation size. A production app must invoke `Terminate` before unloading its code; the current SDK GL surface contract has no quit callback. This is emulator validation, with no physical-device or arbitrary-RetailOS support claim.

The deterministic test plays the full minute to EOF with repeated simultaneous SFX, then repeatedly stops, terminates, reinitialises and seeks the last three seconds. Four buffers give 743 ms of queued latency; this prioritises the existing slow viewer's refill reliability. Seamless production latency and underrun recovery remain future work.

Sixteen screenshot telemetry rows encode: blocks, mixed output frames, accumulated mix microseconds, maximum block mix microseconds, peak active voices, starts, stops, compressed block reads, radio EOFs, errors, audio live bytes, audio peak bytes, play/stop cycles, minimum/maximum warmed free heap with the world fully unloaded, and viewer frame. Each row has 32 little-endian bits at `(10+7*bit,369+4*row)`. Mix timing covers decoding/resampling/mixing, excluding cache cleaning and output submission. NanoEmu guest time is not a cycle-accurate 342 MHz CPU model.

`verify.py` checks live rendering and advancing audio/frame counters, heap bounds, adapter errors, a byte-exact 8,192-frame radio-plus-three-SFX block in the I2S capture, and an exact PCM fingerprint from each of 60 consecutive radio seconds in chronological order. The source disk uses the existing read-only in-memory NAND overlay and its integrity checks.
