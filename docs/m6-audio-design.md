# ViceNano Milestone M6 (Audio) Design Specification
# Platform: iPod nano 7G (Samsung S5L8747, 342/684 MHz ARM11/Cortex-A5, 64 MB RAM, 16 GB NAND)
# Target Output: 44,100 Hz, 16-bit Signed Stereo Interleaved PCM (176,400 B/s)

================================================================================
1. EXECUTIVE SUMMARY & ARCHITECTURE SCOPE
================================================================================
Milestone M6 integrates game audio into the reduced reVC engine running under
NanoEmu and on device. GTA Vice City audio consists of two primary subsystems:
  1. Sound Effects (SFX): Short interactive one-shots and loops (weapons, car
     engines, tire screeches, collisions, UI, footsteps, pedestrian comments,
     and mission speech). In PC 1.0, these are stored in sfx.RAW/SDT and loose WAVs.
  2. Radio Stations & Music Streams: 9 long multi-track station recordings (30 to
     90 minutes each, ~646 minutes total), originally XOR-obfuscated MP3s (.adf),
     plus cutscene/ambience MP3s.

Key Architectural Decisions:
  - Hardware Acceleration: The iPod nano audio subsystem decodes only MP3 in
    realtime via the dedicated media co-processor / ADSP (0x38500000, VIC IRQ 18;
    HLE in NanoEmu via minimp3). All SFX mixing and non-MP3 decoding must run on
    the ARM CPU.
  - SFX Format: Split strategy:
      * Core Bank 0 SFX (568 frequent samples, ~17 MB): Uncompressed 16-bit signed
        mono PCM @ 11.025 / 22.05 kHz. Decode cost = 0 CPU cycles.
      * Pedestrian Comments / Dialogue (9,373 samples, 323 MB): 4-bit IMA ADPCM
        mono @ 22.05 kHz (80.8 MB, 4:1 compression). Decode cost = ~12 cycles/sample.
  - Mixing Budget: Fixed-point software mixer capped at 16 simultaneous SFX
    channels + 1 speech stream + 1 stereo radio stream.
    * CPU Cost @ 342 MHz (Stock): ~4.7% CPU with ADSP MP3 (7.7% with minimp3 fallback).
    * CPU Cost @ 684 MHz (Safe OC): ~2.4% CPU with ADSP MP3 (3.8% with minimp3 fallback).
  - Radio Streaming: De-XOR offline, re-encode music to 64 kbps joint-stereo MP3
    and talk to 32 kbps mono MP3 (shrinking radio from 528.9 MB to 253.0 MB).
    Stream from NAND via a 32 KB double/quad ring buffer (8 KB/s read rate = ~4
    NAND sectors/sec, <0.1% NAND bus saturation).

================================================================================
2. IPOD NANO REALTIME AUDIO PATH CAPABILITIES
================================================================================
The S5L8740/S5L8747 audio architecture (docs/research/audio-path.md) comprises:
  - Output Interface: I2S Controller 0 (MMIO Base 0x3ca00000).
    * TX FIFO Port: 0x3ca00010 (32-bit sample port, 16-bit L/R interleaved).
    * Clock Divisor: Reg +0x40 programmed to 0x110 (272) -> 12,000,000 / 272 =
      44,117.6 Hz (~44.1 kHz standard audio rate).
    * Audio Serial Format: Reg +0x04 programmed via 0x080ba31c.
    * TX Control: Reg +0x08 start ORs with 0x6; stop clears bit 4.
  - DMA Pacing: Multichannel DMA Engine 1 (0x38200000, VIC IRQ 16/17).
    * Peripheral Request Index 12 routes paced transfers to 0x3ca00010.
    * Transfer Granularity: 1024-byte to 2048-byte DMA blocks (512 frames = 11.6 ms
      at 44.1 kHz). Interrupt signals buffer completion; filling runs outside ISR.
  - Codec Control: External audio codec chip (Device ID 0x4281) on SPI0 (0x3c300000,
    VIC IRQ 9). Configured for 44.1 kHz playback (rate code 0x0a).
  - Media Co-processor / ADSP (0x38500000, VIC IRQ 18):
    * The SoC incorporates a dedicated DSP co-processor for audio decompression.
    * Firmware task `MeCCADecode` queues compressed frames to IRAM mailbox at
      0x22038000 (status at 0x2203ff28) and triggers start doorbell (+0x98c = 1).
    * Co-processor decodes MP3 frames into PCM output buffer at 0x22038100
      (1152 * 2 = 2304 s16 stereo halfwords per frame = 4608 bytes).
    * In NanoEmu (emulator/machine/hw/arm/nano7_adsp.c), this co-processor is
      implemented using host-side `minimp3`.
    * In bare-metal or standalone apps, `minimp3` can be linked directly into
      the ARM executable as a lightweight pure C software decoder.

REALTIME DECODE VERDICT:
  - The hardware pipeline natively consumes ONLY 44.1 kHz 16-bit stereo PCM.
  - The ONLY hardware/firmware-accelerated compressed format is MP3 (via ADSP).
  - Neither the I2S block nor the DMA engine performs on-the-fly ADPCM or WAV
    decompression, sample rate conversion, or multi-channel voice mixing.
  - Therefore, all SFX voices MUST be mixed in software on the ARM CPU into a
    single 44.1 kHz stereo PCM buffer before DMA submission.

================================================================================
3. GTA VICE CITY AUDIO ASSET INVENTORY
================================================================================
Total raw assets: 1,221 files, 1,067,780,441 bytes (~1,067.8 MB):
  1. Radio Stations (AUDIO/*.adf):
     - 9 files, 528,868,173 B (528.9 MB).
     - Format: MP3 stream obfuscated with byte XOR 0x22.
     - 7 music stations @ 44.1 kHz stereo 128 kbps CBR (VROCK 70.9 MB, WILD 62.7 MB,
       WAVE 60.5 MB, FEVER 57.9 MB, ESPANT 56.4 MB, FLASH 54.8 MB, EMOTION 54.1 MB).
     - 2 talk stations @ 16/32 kHz stereo 64/112 kbps (KCHAT 47.6 MB, VCPR 39.6 MB).
  2. Sound Effects Banks (AUDIO/sfx.SDT and sfx.RAW):
     - sfx.RAW: 340,444,322 B (340.4 MB) uncompressed signed 16-bit mono PCM.
     - Bank 0 (Core SFX): 17.0 MB, 568 samples (weapons, cars, hits, footsteps, UI).
       Sample rates: 81% @ 12 kHz, 13% @ 16 kHz, 2% @ 22 kHz, 4% other.
     - Bank 1 (Pedestrian comments): 323.2 MB, 9,373 samples (speech lines).
  3. Loose Speech & Cutscenes:
     - 90 MP3 streams (160.3 MB): 71 cutscene tracks @ 32 kHz stereo 112 kbps;
       13 ambient tracks @ 32 kHz 128 kbps; 6 misc/police tracks.
     - 1,120 WAV streams (38.2 MB): 1,090 files are 4-bit IMA ADPCM mono @ 22.05 kHz
       (36.2 MB); 30 files are 16-bit PCM mono @ 11/22 kHz (2.0 MB).

================================================================================
4. CONVERSION PIPELINE FOR VC SFX (CHEAPEST DECODE FORMAT)
================================================================================
A. Decode Cost vs. Storage Analysis:
  - Format Option 1: Uncompressed 16-bit Signed PCM
    * Decode Cost: 0 cycles. Direct sample pointer fetch.
    * CPU Impact: Minimum possible.
    * Storage Impact: 340 MB sfx.RAW is large, but Bank 0 alone is only 17.0 MB.
  - Format Option 2: 4-bit IMA ADPCM (DVI / WAV standard)
    * Decode Cost: 10–14 cycles per sample on ARM11. Uses standard 89-entry step
      table and 8-entry index adaptation table. No multiplies, no floats.
    * Compression Ratio: 4:1 vs 16-bit PCM (4 bits/sample vs 16 bits/sample).
    * Storage Impact: Shrinks 323 MB ped speech to 80.8 MB!
  - Format Option 3: MP3 / Ogg Vorbis
    * Decode Cost: 250–400 cycles per sample on ARM CPU (~10–14 MHz per stream).
    * Unsuitable for polyphonic SFX (16 voices would consume 100% of the CPU).

B. Recommended SFX Split Strategy:
  1. Bank 0 (Core Gameplay SFX):
     - Keep as 16-bit Mono Signed PCM.
     - Offline Preprocessing: Resample all 568 samples to fixed base rates
       (11,025 Hz or 22,050 Hz) using polyphase filter on PC host. This eliminates
       run-time resampling drift and complex interpolation edge cases.
     - Size: ~17.0 MB on NAND. Preload frequently used samples into a 1–2 MB
       RAM cache at boot/scene load. Decode overhead = 0%.
  2. Bank 1 (Ped Comments & Ambient Speech):
     - Convert to 4-bit IMA ADPCM Mono @ 22.05 kHz.
     - Chunking: Independent 256-sample blocks (132 bytes per block: 2-byte initial
       sample + 1-byte step index + 1-byte pad + 128 bytes nibble payload).
     - Size: 80.8 MB on NAND (-75% reduction).
     - Streaming: Decoded on-the-fly into a small 2 KB voice scratch ring buffer only
       when a ped speaks. Single speech stream @ 22.05 kHz costs <0.1% CPU.
  3. Total SFX NAND Footprint: 97.8 MB (down from 340.4 MB, saving 242.6 MB).

C. Host Conversion Pipeline Tool (`tools/vc-audio-pack.py`):
  - Step 1: Parse original `sfx.SDT` index (offset, size, frequency, loop points)
    and extract PCM slices from `sfx.RAW`.
  - Step 2: Separate Bank 0 (core) and Bank 1 (ped comments).
  - Step 3: Bank 0 samples resampled to 11.025 / 22.05 kHz, packed into `sfx_bank0.bin`
    with sector-aligned 2048-byte offsets. Generate compact lookup table `sfx_bank0.idx`.
  - Step 4: Bank 1 samples encoded into 4-bit IMA ADPCM with 256-sample block headers,
    packed into `sfx_ped.bin` with `sfx_ped.idx`.
  - Step 5: Convert WAV mission dialogues to standard IMA ADPCM mono chunks.

================================================================================
5. MIXING BUDGET & CPU COST ON 342 / 684 MHz ARM11
================================================================================
A. Software Mixer Requirements:
  - Output Target: 44,100 Hz stereo interleaved s16 (44,100 frames/sec).
  - Per Voice Inner Loop:
    1. Phase Accumulation: `phase += freq_step;` (16.16 fixed point) -> 1 cycle.
    2. Sample Fetch: Load 16-bit sample from memory -> 2–3 cycles.
    3. Linear Interpolation:
       `smp = s0 + (((s1 - s0) * (phase & 0xFFFF)) >> 16);` -> 3 cycles (SMULWB/ADD).
    4. Panning & Volume Attenuation:
       `left = (smp * vol_l) >> 15; right = (smp * vol_r) >> 15;` -> 2 cycles (SMULWB).
    5. Accumulate into 32-bit Stereo Mix Buffer:
       `acc_l += left; acc_r += right;` -> 2 cycles.
    6. Pointer updates, loop branch & cache management -> 4–6 cycles.
  - Conservative Inner Loop Cost: 18–22 cycles per voice per output frame on ARM11.
    Master Pass Cost (saturation clamping with SSAT + packing): ~5 cycles/frame.

B. Voice Budget vs. CPU Load Matrix (at 44.1 kHz, 22 cycles/voice-frame):
  -----------------------------------------------------------------------------
  Simultaneous Channels | MHz Required | % CPU @ 342 MHz (Stock) | % CPU @ 684 MHz (OC)
  -----------------------------------------------------------------------------
   8 SFX voices         |   8.0 MHz    |          2.3%           |        1.2%
  12 SFX voices         |  11.9 MHz    |          3.5%           |        1.7%
  16 SFX voices         |  15.7 MHz    |          4.6%           |        2.3%
  24 SFX voices         |  23.5 MHz    |          6.9%           |        3.4%
  32 SFX voices         |  31.3 MHz    |          9.1%           |        4.6%
  43 SFX voices (PS2)   |  41.9 MHz    |         12.3%           |        6.1%
  -----------------------------------------------------------------------------

C. Recommended Mixing Architecture:
  - Cap Active SFX Voices to 16 Channels:
    * 16 generic voices provide complete audio coverage for heavy gameplay
      (engine + tires + 4 weapon sounds + 4 explosions/hits + 4 peds/ambient + UI).
    * AudioManager priority culling drops inaudible/low-volume sounds automatically.
    * Base Mixing Cost: 15.7 MHz (~4.6% @ 342 MHz / 2.3% @ 684 MHz).
  - Effects Cuts & Optimizations:
    * Cut EAX reverb, reflections, and environmental DSP filters (saves ~4–6% CPU).
    * Cut HRTF 3D spatial resamplers. Use fast 2D stereo pan table and `1 / (1 + dist)`
      distance attenuation.
    * Use ARMv6 SIMD / DSP instructions: `SMULWB`, `SMLAWB`, `SSAT`, `PKHBT`.
    * If running on Cortex-A5 with NEON, vectorizing the stereo accumulation
      and saturation pass drops the master mix cost to <0.5 MHz.

================================================================================
6. RADIO STREAMING PLAN FROM NAND
================================================================================
A. Storage & Bitrate Re-encoding:
  - De-XOR: Strip XOR 0x22 obfuscation offline during host packaging.
  - Re-encode Profile:
    * 7 Music Stations: Re-encode from 128 kbps to 64 kbps joint-stereo MP3 @ 44.1 kHz.
      (Quality transparent on iPod nano headphone output and internal speaker).
    * 2 Talk Stations (KCHAT, VCPR): Re-encode from 64/112 kbps to 32 kbps mono MP3 @ 22.05 kHz.
    * Storage Savings: 528.9 MB -> 253.0 MB (52% reduction, saves 275.9 MB flash).
    * Total Game NAND Footprint: Assets ~472 MB + Audio ~465 MB = 937 MB (<1 GB).
      (Easily fits on the 16 GB NAND with >13.5 GB free).

B. NAND Bandwidth & I/O Pacing:
  - Streaming Bitrate: 64 kbps = 8,000 bytes/second.
  - Sector Transfer: 8 KB/s corresponds to exactly four 2048-byte NAND sectors per second.
  - Comparison to World Streaming: World geometry/texture streaming reads at 2–5 MB/s
    during fast driving. Radio streaming represents <0.2% of NAND bus bandwidth.
  - Zero Stalls: Because audio read demands are negligible, radio I/O can be scheduled
    synchronously or in small 8 KB bursts without causing visual streaming hiccups.

C. RAM Buffer Architecture:
  - Quad-Block Ping-Pong Ring Buffer:
    * 4 blocks × 8,192 bytes = 32 KB total RAM allocation.
    * Holds 4.096 seconds of compressed audio at 64 kbps (or 2.048 s at 128 kbps).
    * Watermark Logic: When available buffer drops below 16 KB (2 seconds of audio),
      trigger asynchronous read of next 8 KB block via `hb_fs_read` / `CdStreamRead`.
    * Memory Footprint: 32 KB is negligible (<0.05% of 64 MB RAM, fits well inside
      the 3 MiB audio allocation budget from docs/exec-plan.md).

D. Decode Execution:
  - Hardware ADSP Co-processor Path (Primary on RetailOS / NanoEmu):
    * Feeds compressed frames from ring buffer into IRAM mailbox (0x22038000).
    * Co-processor decodes frame in ~20 µs and asserts pending bit 2 / IRQ 18.
    * CPU Cost: <0.1% CPU (negligible).
  - Software Fallback Path (minimp3 on ARM CPU):
    * Decodes 1 MP3 frame (1152 samples = ~26.1 ms) every 26 ms.
    * Cycle Cost: ~240,000 cycles per frame -> ~9.2 MHz CPU load.
    * CPU Cost: ~2.7% @ 342 MHz / ~1.3% @ 684 MHz.

E. Virtual Station Clock & Track Switching:
  - Persistent Virtual Time Tracking: `MusicManager` maintains virtual elapsed playback
    time for all 9 stations simultaneously via a millisecond counter:
    `station_offset_ms = (total_game_time_ms) % station_duration_ms;`
  - Instant Station Seek:
    * Store pre-computed seek table (1 entry every 10 seconds: NAND sector offset +
      MP3 frame bit reservoir sync state).
    * Upon entering vehicle or switching station: immediately flush 32 KB buffer,
      seek NAND file handle to corresponding sector, prime ring buffer, and start decode.
    * Seek latency is under 15 ms (one NAND read).

================================================================================
7. REVC ENGINE INTEGRATION PLAN
================================================================================
A. Subsystem Mapping in `games/reVC/src/audio/`:
  - `DMAudio.cpp`: Game-facing API (car radio, one-shots, mission audio, volume).
  - `AudioManager.cpp`: Sound scene manager, voice priority sorting, distance attenuation.
  - `AudioLogic.cpp`: High-level sound triggering (footsteps, vehicle engine RPM).
  - `MusicManager.cpp`: Radio station selector, playback offsets, fades, cutscenes.
  - `sampman.h`: Core abstraction layer.

B. New Backend: `sampman_nano.cpp`:
  - Implements `cSampleManager` replacing `sampman_oal.cpp` and `sampman_miles.cpp`.
  - Responsibilities:
    1. Sample Bank Loading: Parse converted `sfx_core.idx` and mmap/read `sfx_core.bin`.
    2. Voice State Array: Maintain 16 voice structures:
       `{ uint8_t *data; uint32_t len; uint32_t phase; uint32_t step; uint16_t vol_l, vol_r; bool loop; }`
    3. Channel Control API: `InitialiseChannel`, `SetChannelVolume`, `SetChannelPan`,
       `SetChannelFrequency`, `StartChannel`, `StopChannel`.
    4. Software Mixer Callback: Invoked by timer or audio worker thread to generate
       512 frames (2048 bytes) of s16 stereo audio.
    5. Stream Manager: Feeds ADSP co-processor / minimp3 from NAND ring buffer for radio.
    6. DMA Bridge: Submits mixed 2048-byte buffers to DMA Engine 1 / I2S0 FIFO.

================================================================================
8. TOTAL CPU & MEMORY BUDGET SUMMARY
================================================================================
A. Total Audio CPU Budget:
  -----------------------------------------------------------------------------
  Component                      | MHz @ 44.1kHz | % CPU @ 342 MHz | % CPU @ 684 MHz
  -----------------------------------------------------------------------------
  16 SFX Voices (16-bit PCM)     |   15.7 MHz    |      4.6%       |      2.3%
  Master Stereo Clamping & DMA   |    0.5 MHz    |      0.1%       |      0.1%
  1 Ped/Speech Stream (ADPCM)    |    0.3 MHz    |      0.1%       |      0.05%
  Radio Stream (ADSP HW decode)  |   <0.1 MHz    |     <0.1%       |     <0.1%
  Radio Stream (minimp3 SW fall) |    9.2 MHz    |      2.7%       |      1.3%
  -----------------------------------------------------------------------------
  TOTAL AUDIO CPU (ADSP Path):   |   16.6 MHz    |      4.8%       |      2.4%
  TOTAL AUDIO CPU (SW Fallback): |   25.7 MHz    |      7.5%       |      3.7%
  -----------------------------------------------------------------------------

B. Total Audio Memory Budget:
  - Flash / NAND Storage:
    * Radio Stations (9 stations @ 64k/32k MP3): 253.0 MB
    * Core SFX Bank 0 (16-bit PCM): 17.0 MB
    * Ped Comments Bank 1 (4-bit IMA ADPCM): 80.8 MB
    * Cutscene / Ambience MP3s: 77.0 MB
    * Mission Dialogue (IMA ADPCM): 37.0 MB
    * Total NAND Audio Footprint: 464.8 MB (down from 1,067.8 MB, 56.5% savings).
  - RAM Residency:
    * Core SFX Active Sound Cache: 1.50 MB
    * Radio Stream Ring Buffer: 0.03 MB (32 KB)
    * Speech Stream Scratch Buffer: 0.01 MB (8 KB)
    * Mixer DMA Double Buffers (4 × 2 KB): 0.01 MB (8 KB)
    * Total RAM Audio Footprint: ~1.55 MB (well under the 3.0 MB budget in exec-plan.md).
