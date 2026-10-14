hardware.

Use an **OS-launched chainloader**, when ready; the corrected boot analysis does not establish unsigned cold-boot diagnostics loading.
Evidence: [RAM analysis](/home/pavliukov/ipodwork/NanoRework/docs/research/ram-map.md), [ViceNano plan](/home/pavliukov/ipodwork/ViceNano/docs/exec-plan.md), [chainloader design](/home/pavliukov/ipodwork/NanoRework/docs/research/chainloader-design.md), [corrected boot analysis](/home/pavliukov/ipodwork/NanoRework/docs/research/llb-analysis-codex.md).
tokens used
39,216
**ViceNano decision: prototype as a NanoApps `.hbapp`; pursue chainloading only after measured need and driver readiness.**

| Factor | A — `.hbapp` in RetailOS | B — bare-metal chainloader |
|---|---|---|
| Memory | Shares 40.299 MiB with OS; available space varies and fragments. One snapshot had only 2.653 MiB free. | Owns 64 MiB after verified takeover; game code, drivers, buffers and GPU allocations still consume it. |
| Vice City streaming | Proposed 40 MiB game budget is unproven and cannot assume the whole OS heap is available. | More room for textures, geometry, entities and loading peaks; bounded streaming remains essential. |
| GPU | `GL_SURFACE` supplies hardware GLES 1.1 through RetailOS; OS handles context and presentation, as for MK64/SM64. | Direct MBX ownership requires command submission, GPU memory mapping, synchronization and display drivers; SDK GL calls become unusable. |
| Audio | OS mixer/output exists, but SDK WAV playback is limited; continuous mixed effects/radio need integration. | Implement software mixing/decoding plus SPI codec control, I2S, DMA, clocks and volume routing. |
| Input | Reuse OS touch/button events and NanoApps input adapter. | Implement raw touch/button drivers and event timing; Home becomes a game-controlled action. |
| Install/exit | Standard app pack, executable and separate asset files; Home exits to RetailOS. | NanoApps launcher plus payload/assets; committed takeover exits by rebooting into RetailOS. |
| NanoEmu cycle | Fastest first playable path: replace `.hbapp`, resume RetailOS snapshot, exercise existing GLES/MBX models. | Test takeover and drivers before gameplay; direct payload loading accelerates iteration but does not validate real handoff. |

Vice City needs a resident **working set**, not the entire archive in RAM: downscale textures, bound model/vehicle pools, evict aggressively, and count decoded CPU/GPU copies and transition peaks.
Replace reVC’s default 65 MiB streaming assignments; verify random-access IMG reads—current `hb_fs_read` starts at the beginning.
Treat the older 24–32 MiB texture target and newer 40 MiB total budget as proposals; validate formats/extensions and actual residency.

1. **Prototype:** ship `ViceNano.hbapp` with a GLES cube → VC asset viewer → reduced streamed world; initially use silent audio and touch/buttons.
2. **Measure:** instrument total free/largest block, code/BSS, GPU allocations, upload duplication and worst-case dense-area traversal on hardware.
3. **Decision gate:** retain `.hbapp` if representative gameplay fits with OS headroom and acceptable stalls; otherwise reduce assets/residency and assess chainloading.
4. **Graduate:** keep renderer/audio/input/storage behind platform interfaces; implement bare-metal MBX/display, audio, input and NAND/FTL read access before switching.
5. **Release gate:** validate DMA drainage, watchdog, caches/MMU, full-RAM ownership, asset streaming and reboot recovery in NanoEmu and on hardware.

Use an **OS-launched chainloader**, when ready; the corrected boot analysis does not establish unsigned cold-boot diagnostics loading.
Evidence: [RAM analysis](/home/pavliukov/ipodwork/NanoRework/docs/research/ram-map.md), [ViceNano plan](/home/pavliukov/ipodwork/ViceNano/docs/exec-plan.md), [chainloader design](/home/pavliukov/ipodwork/NanoRework/docs/research/chainloader-design.md), [corrected boot analysis](/home/pavliukov/ipodwork/NanoRework/docs/research/llb-analysis-codex.md).
