# ViceNano Audio Compression & Decode Strategy (iPod nano 7G: Cortex-A5, ADSP, 64MB RAM)
1. INVENTORY (1,221 files, 1,067,780,441 B = 1,067.8 MB total):
   - Radio: 528,868,173 B (528.9 MB, 9 .adf files: 7 music @ 437.5 MB + 2 talk @ 91.4 MB).
   - SFX Bank: 340,444,322 B (340.4 MB, sfx.RAW/SDT: 17.0 MB core SFX [568 smp] + 323.2 MB ped comments [9,373 smp]).
   - Speech/Streams: 198,467,946 B (198.5 MB: 90 .mp3 cutscene/amb @ 160.3 MB + 1,120 .wav mission dialogue @ 38.2 MB).
2. CURRENT FORMATS & COMPRESSION:
   - Radio (.adf): MP3 XOR 0x22; 7 music @ 44.1kHz stereo 128kbps CBR; 2 talk @ 16/32kHz stereo 64/112kbps.
   - SFX (sfx.RAW): Uncompressed 16-bit signed PCM mono; variable rates (81% 12kHz, 13% 16kHz, 2% 22kHz, 4% other).
   - MP3 Streams: 71 cutscenes @ 32kHz stereo 112kbps; 13 ambience @ 32kHz 128kbps; 6 police/taxi/misc @ 22-44.1kHz.
   - WAV Streams: 1,090 files 4-bit IMA ADPCM mono @ 22.05kHz (36.2 MB); 30 files 16-bit PCM mono @ 11/22kHz (2.0 MB).
3. TARGET FORMAT RECOMMENDATIONS & SIZES:
   - Radio -> ADSP HW MP3: De-XOR to MP3; re-encode music @ 64kbps joint-stereo, talk @ 32kbps mono -> 253.0 MB (-52%).
   - SFX -> CPU PCM Mixer: Core SFX (Bank 0) stays 16-bit PCM mono (17.0 MB) in RAM cache; compress ped comments to 4-bit IMA ADPCM -> 80.8 MB (SFX total: 98.0 MB, -71%).
   - Speech/Streams -> Split ADSP / CPU: Cutscenes/Ambience to 64kbps MP3 (ADSP when radio stopped, 77.0 MB); dialogue WAVs keep 4-bit IMA ADPCM @ 22kHz (37.0 MB) (Speech total: 114.0 MB, -43%).
   - Target Total Size: 465.0 MB (56.5% reduction, saves 602.8 MB NAND; keeps 100% of game audio).
4. CORTEX-A5 DECODE CPU BUDGET (Single-core @ 400-684 MHz, 44.1kHz s16 stereo DMA output):
   - Radio (ADSP co-processor @ 0x38500000, IRQ 18): <0.1% CPU (~20µs mailbox handshake); minimp3 SW fallback = 1.6-3.0%.
   - Speech (IMA ADPCM on CPU): ~0.05% CPU (simple integer step table, <0.2 MHz per stream for 22kHz mono).
   - SFX Mixer (NEON linear interp + stereo pan + sat): 16 voices @ 44.1kHz = 10.5-14.1 MHz -> 2.1-3.5% CPU (0% decode overhead). Total Audio CPU < 4.0%.
