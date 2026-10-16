# ViceNano: plan to reach 30 FPS on the iPod nano 7G

Date: 2026-10-15. Status: plan only. No code has been changed. Every number marked *NanoEmu* is emulator evidence and still has to be checked on the device.

> **Update, same day: P0, P1 and P2 have been implemented and measured** ([m8/p0p2-runs/RESULTS.md](../m8/p0p2-runs/RESULTS.md)). Diagnosis point 1 below is **refuted**: `glDrawElements` and `glDrawArrays` both cost about 0.1 ms per draw. P1 gives no gain and adds ~2.7 ms in NanoEmu. P2 cuts overlay time from ~3 to ~1 ms, but FPS doesn't move. The 14 world draws take 14–38 ms because NanoEmu runs its VGP interpreter synchronously in the guest MMIO path, and the guest clock follows host time. **NanoEmu FPS therefore can't validate these optimisations.** The next step is a device measurement, or an emulator timing fix, before P3 and later items.

> **Follow-up, same day: P1 reverted and NanoEmu timing fixed.** Indexed `glDrawElements` is the default again. The de-indexed path is opt-in with `-DM8_ARRAY_DRAWS`, and only that build allocates the flat buffer.
>
> **Profiling** with a new exact-PC guest profiler (`NANO7_PROF`) showed that the world draw is spent in the RetailOS driver's display-list interpreter (`0x0817a444`). It streams about 17.5k single-word stores per frame into the TA FIFO aperture, and in QEMU each store costs ~0.6 µs of slow-path and BQL overhead against a posted write on hardware. VGP interpretation and rasterisation add another ~6–7 ms per frame.
>
> **The fix:** NanoEmu now excludes all of that from guest time. It charges modelled time instead: 4 CPU cycles per FIFO word, a TA at the GPU clock that the CPU waits on only when the FIFO is full, and a render job of 1 cycle per fragment. `NANO7_MBX_HOST_TIME=1` restores the old timing.
>
> **Result:** the world draw falls from 14–17 ms to 8.2 ms and the harness reports 26–29 FPS. The app's frame work is now ~9 ms, but the frame interval is still ~50 ms (3 vsyncs), with the CPU idle in RetailOS's `wfi` loop for ~25 ms per frame. **The next bottleneck is what gates frame start or present, not rendering.** Under modelled timing the array path is faster (5.3 ms against 8.3 ms), so the P1 question goes back to the device. CPU time is still host-timed (no icount), so NanoEmu can't rank CPU-side changes. Details: [m8/p0p2-runs/RESULTS.md](../m8/p0p2-runs/RESULTS.md#emulator-timing-fix).

> **Follow-up 2 (2026-10-16): the frame gate is the heartbeat timer, which is ~2× slow in the emulator.** The resident's 16 ms one-shot `Inval` heartbeat fires a median 27.6 ms after re-arm. RetailOS's kernel treats a timed wait's ms as scheduler ticks (and keeps 1000 ticks per second), but its timer init programs a 500 Hz tick from a 3 MHz constant, and NanoEmu emulates that literally. With the tick at 1 kHz (`NANO7_TIMER_MHZ=6`), M8 reaches 54.7 FPS and 99.7% of frames land within 2 vsyncs. At the current 500 Hz it's 34.6 FPS (after fixing two more emulator bugs, in render-job and display-pipe completion timing). **The deciding next step is a device probe of the tick rate.** If the device ticks at 500 Hz, the resident's heartbeat should be 8 ms or TE-paced, and that would lift every GL app. Details: [m8/p0p2-runs/RESULTS.md](../m8/p0p2-runs/RESULTS.md#what-gates-the-frame-start-2026-10-16).

## 1. The diagnosis in one paragraph

M8 renders a tiny scene: 14–21 draws, about 2,400 vertices, 26 textures of 32×32, and the GPU is busy only about 8% of the time (`nano7-mbxr perf: render 3.6 ms/frame (8 %)`). It still runs at about 20 FPS. Stock clocks (342/133 MHz, 19.97 FPS) and unlocked clocks (684/266 MHz, 20.56 FPS) give nearly the same result. **A frame rate that doesn't change when the CPU and GPU clocks double means the frame is bound by latency and synchronisation, not by compute.** Three things cause it:

1. **`glDrawElements` is synchronous.** `ViceNano/sdk/gl.c:332-376` queues each indexed draw to the RetailOS MBX flush thread, then blocks on `A_OS_WAITSEM` until that draw is finished. The measured draw section is 15.7 ms for 14 draws, about **1.1 ms per draw**. The vertex work is trivial, so most of that is the thread round trip, which plausibly costs one scheduler tick per draw. This hypothesis still needs the micro-benchmark in step P0. Every world draw goes through this path (`m8/model.cpp:283`).
2. **The display quantises the frame period.** The panel is a command-mode DSI panel with a 60 Hz TE pulse (`NanoEmu/docs/research/display-cap.md`), and the resident drives frames from a 16 ms timer that triggers a view `Inval` (`silver_resident.c:407,529`). A frame therefore lasts a whole number of vsync periods: 16.7 ms (60 FPS), 33.3 ms (30 FPS) or 50 ms (20 FPS). M8's ~20 FPS is the 3-vsync bucket, and the 23 FPS "peaks" are frames alternating between 2 and 3 vsyncs. **To lock 30 FPS, CPU work plus GPU render plus composite must reliably fit in about 30 ms.**
3. **Debug overhead and noise push frames into the 3-vsync bucket.** The telemetry and bit-row overlay (`m8/viewer.cpp`, a `tile()` per lit bit and per glyph run) sends **about 1.26× as many vertices as the world**: 1,076,376 untextured vertices against 857,248 textured ones in the 01-max-baseline MBX state log. Audio mixing runs on the render thread with spikes of 1.3–3.9 ms (`capabilities.json` frames). Player movement uses a fixed step per frame, so runs are not comparable.

How Mario Kart 64 (nano7-untethered) holds 30 FPS on the same device at about 320 MHz with 60–120 draws per frame: it **never calls `glDrawElements`**. It submits through client arrays and `glDrawArrays` at about 0.1 ms per draw, coalesces draws by state, shadows GL state, defers opaque draws to the end of the frame, uses 16bpp textures only, keeps at most 72 live textures, and **paces on the vsync tick explicitly** (`mk64_entry.c:239-246`: the game ticks every second vsync and `port_game_replay()` redraws the cached frame in between). Its frames take 16–20 ms (`games/README.md:32`).

## 2. Frame budget for a locked 30 FPS (33.3 ms)

| Slice | M8 now (NanoEmu, unlocked) | Target for the harness | Target for the full game |
|---|---:|---:|---:|
| GL submission (world) | 13.0–21.4 ms | ≤ 3 ms | ≤ 8 ms (≤ 80 draws) |
| HUD / overlay | not attributed (≈ world vertex volume) | ≤ 0.5 ms | ≤ 1.5 ms |
| Simulation (physics, AI, anim, skinning) | ≈ 0 (marker only) | — | ≤ 12 ms |
| Audio on the render thread | 0.3 avg, spikes up to 3.9 ms | ≈ 0 (moved off) | ≤ 1 ms |
| Streaming | < 0.05 ms | — | ≤ 2 ms amortised |
| GPU render (overlaps the next CPU frame) | ~3.6 ms host-equivalent | — | ≤ 20 ms |
| Slack for TE alignment / compositor | — | ≥ 8 ms | ≥ 5 ms |

## 3. Prioritised action plan

Gains are expected FPS on the M8 harness scenario unless a row says "full game". Risk covers both correctness and the hardware.

| # | Action | Expected gain | Risk | Effort |
|---|---|---|---|---|
| P0 | Measurement fixes (prerequisite) | none; makes the other gains measurable | very low | 1–2 d |
| **P1** | **Replace `glDrawElements` with de-indexed client arrays + `glDrawArrays`** | **~20 → 30 locked**; draw time 16 → ~2–4 ms | low–med | 2–3 d |
| **P2** | **Strip or cache the debug overlay in performance builds** | +1–4 ms; removes ~55% of submitted vertices | very low | 0.5 d |
| **P3** | **vsync-aligned 30 Hz pacing + fixed-step simulation** | turns 20/23 judder into a stable 30; required for honest measurement | low | 1–2 d |
| **P4** | **Move audio mixing off the render thread** | removes 1–4 ms spikes that cause missed vsyncs | low–med | 2–3 d |
| P5 | Texture atlases per streamed sector + state sort + ≤72 live textures | full game: keeps draws ≤ 80 and avoids the driver panic above 72–80 textures | medium | 1–2 w |
| **P6** | **Vertex budget, LOD and draw distance** (≤ ~6k opaque + 2k alpha vertices per scene) | full game: prevents a parameter-buffer overflow assert and caps TA cost | medium | 1–2 w |
| P7 | Simulation scheduling: 30 Hz near, 10–15 Hz far; ped and car caps; cheaper collision | full game: 5–15 ms saved under dense traffic | medium | 2–4 w |
| P8 | Codegen: hardware-float libm, fix LTO, VFPv4-D16 hot paths | 2–10% CPU, bigger in math-heavy code | low | 1–2 d |
| P9 | PVRTC 4bpp world textures | full game: 4× less texture memory and bandwidth than 16bpp; ~0 on the harness | medium | 1–2 w (+ NanoEmu decoder) |
| P10 | IRAM hot loops, L1 prefetch, governor lock | 5–15% on targeted loops | medium | 1 w |
| P11 | Clocks: CPU 684 MHz, GPU at 171 MHz (stock table) | ~0 on the harness; full game: CPU headroom | med–high (hardware) | — |
| ✗ | Resolution scaling | rejected: panel fixed at 240×432, GPU not fill-bound | — | — |

### P0: make the measurements trustworthy (do first)

- Add `totalFrameUs`, `lastOverlayUs`, the inter-frame interval, and a **vsync-bucket histogram** (counts of 1, 2, 3 and 4+ TE intervals) to the telemetry bit rows. Today the `[M8-PROFILE]` line goes to stderr, which the runner doesn't capture, so about 30 ms of the 48 ms frame is unattributed.
- Time `RwCameraEndUpdate` and the resident's end-scene separately to separate GPU or composite waiting from CPU work.
- **Draw-path micro-benchmark:** N×1-triangle draws through `glDrawElements` against `glDrawArrays`, for N = 1, 8, 32. This confirms or refutes the ~1 ms per draw blocking cost. Note that `hb_gl_drawelements_native(0)` in `ViceNano/sdk/gl.c` currently makes `glDrawElements` a **no-op**, despite the comment in the upstream header, so it can't be used as an A/B switch.
- Make movement time-based and replay a recorded input trace so camera paths match between builds. Run at least 3 repeats per variant. OPTIMIZATION_REPORT shows the same binary at 16.4 and 23.3 FPS.
- Plan device verification. NanoEmu timing for semaphores, the flush thread and the TE pulse is modelled, not silicon. Any GO decision needs a device run, keeping the dualboot and pack constraints in mind: `start install` is destructive.

### P1: remove the synchronous indexed draw (largest single win)

- When residency changes (already handled in `m8PrepareBatches`), expand each material group's ushort indices into a contiguous `GL_TRIANGLES` vertex stream. Draw it from client arrays with `glDrawArrays`, the path MK64 uses at about 0.1 ms per draw (gpu-deep-dive rank 3, mk64-renderer-tricks §1).
- Memory cost: 4,356 indices × 24 B ≈ 105 KB, against 57 KB indexed. That's negligible against a 6 MB free heap. Shrink the vertex while doing this: `GL_SHORT` positions with a per-pack scale in the modelview, `GL_SHORT` texcoords, and RGBA8 colour give **12–16 B per vertex**, which cuts the bandwidth the driver copies.
- Apply the same change in librw's `gles1pipe.cpp:59` and `gles1immed.cpp:16,38` before the full reVC renderer goes live, and collapse tristrips into lists there.
- Optional next step: `glMultiDrawArraysEXT`, which the driver exposes, can submit all opaque groups that share a texture in one call.
- Expected: with 14 draws × ~0.15 ms plus driver vertex work, the draw section falls from ~16 ms to ~2–4 ms. Total frame work drops well under 33 ms and **the frame lands in the 2-vsync (30 FPS) bucket**. It may reach 60 on this toy scene.
- Risks:
  - Confirm that `glDrawArrays` with client arrays doesn't also round-trip on this driver build (P0 micro-benchmark).
  - De-indexing raises the vertex count, which interacts with P6's budget.
  - MK64 found that texture uploads between draws can reboot the driver, so keep all uploads before the first draw of the frame. M8 already does this.

### P2: debug overlay

- Compile the bit-row telemetry and button labels only in the verifier build (`-DM8_TELEMETRY`).
- For the shipping HUD, render the static button art once into a texture and draw it with `glDrawTexiOES`, or with ≤ 10 textured quads. Update FPS and text at most every 250 ms into a small cached texture.
- Expected: removes roughly half of all submitted vertices and some fill (the 224×112 telemetry plate).
- Caveat: the M8 verifier decodes the bit rows. Keep a telemetry build for the gates and add a release-config run that measures FPS through the frame-interval log.

### P3: pacing

- Run simulation at a fixed 30 Hz step, like reVC's `CTimer` with `ms_fTimeStep` held at 1/30 × 50. Render on every second TE pulse. If the next heartbeat arrives early, replay the last command list (MK64's `force_30fps` with `port_game_replay`), or have ViceNano's frame function return without simulating.
- Consider a 33 ms heartbeat in the resident for ViceNano, or skip every other `gl_app_frame`. Check whether the view must be redrawn on every `Inval` (MK64 replays rather than skipping), so the screen doesn't flash.
- Expected: a stable 30 instead of a 20–25 average with uneven frame times. Measure the 1% lows, not only the mean.

### P4: audio

- Run `DMAudio.Service()` and mixing from the DMA-complete interrupt's deferred worker, or from a dedicated low-priority RetailOS task, so the render thread never mixes.
- Keep 8192-frame buffers. Shrinking them to 2048 cost ~0.6 ms per frame in trial 05.
- Mix SFX at 22.05 kHz and upsample once. Keep the radio pre-encoded, and decode it in larger chunks less often.
- Risks: VFP context on the audio task requires `hb_fpu_attach_current_task` there too, and locking between the game and the mixer queue.

### P5: batching for the full game

- GTA scenes reference hundreds of textures, and the driver panics above about 72–80 live textures (MK64: `LIVE_TEX_HARD = 72`). **Atlasing is mandatory, not optional.**
- In the converter, pack each streamed sector's TXD into 256×256 16bpp (or PVRTC) atlas pages, rewrite UVs, and split any wrap-mode textures.
- Sort opaque geometry by atlas page, roughly front to back. Then draw alpha last, sorted back to front with `glDepthMask(GL_FALSE)`. Keep `GL_ALPHA_TEST` off so hidden-surface removal stays active (gpu-deep-dive rank 4).
- Target ≤ 80 draws and ≤ 48 live textures per frame.

### P6: vertex budget, culling, LOD, draw distance

- RetailOS 1.1.2 asserts when the TA parameter buffer overflows (3.75 MB, `0x080b1532` depth-store bridge not implemented, gpu-deep-dive rank 5). MK64 caps at 6,144 opaque + 3,072 alpha vertices. **Find the real ceiling with a device ramp test**, then enforce it in the renderer with a hard cap that degrades LOD instead of crashing.
- Set reVC draw distance to roughly 150–250 world units. Lower `CRenderer` LOD distances (`ms_lodDistScale` ≈ 0.5–0.7) and swap to LOD models early. Bake distance fog into vertex colours on the CPU, as MK64 does, rather than relying on GL fog.
- Cull per sector and per material sphere (already in M8). Add a CPU cull of degenerate triangles under 0.25 px: these crash the driver's triangle setup.
- Trial 03 showed the far plane is irrelevant on the harness (draws stayed at 14). This item matters only for the full game.

### P7: simulation (full game)

From `docs/vcphys.md` and `docs/agy-vcfps.txt`, with one correction: **NEON is unavailable**. The kernel sets CPACR ASEDIS and D32DIS (cpu-deep-dive §2). Every "NEON skinning/collision" item in earlier notes becomes scalar VFPv4-D16 or VGP work.

- Run full physics on the player and nearby entities at 30 Hz. Put distant cars on rails (`STATUS_SIMPLE`). Tick distant peds and cars at 10–15 Hz.
- Use one or two `CWorld::Process` retry passes for NPCs.
- Enable `COMPRESSED_COL_VECTORS`. Add per-mesh triangle clusters before `ProcessColModels`.
- Cap population at about 8 peds and 6 cars.
- CPU skinning: rigid or single-bone LOD beyond ~15 m, and update far animation at 15 Hz. VGP matrix-palette skinning is a HIGH-risk stretch item.

### P8: codegen (cheap)

- `m8/Makefile` links `newlib/thumb/nofp/libm.a`, so `sinf`, `cosf` and `sqrtf` are **software float**. Switch to `newlib/thumb/v7-a+fp/softfp`, which is installed and keeps the softfp ABI. This is a one-line, low-risk change.
- Investigate the white or corrupt frames under LTO (trial 07). They are likely a section, constructor or relocation issue under `-mword-relocations`, not a compiler bug. LTO usually gives about 5%.
- Keep `-O3 -ffast-math`. Keep FPSCR in flush-to-zero / default-NaN mode (RunFast).

### P9: textures

- PAL8 already removed the CPU shadow copy, but **the driver expands PAL8 to 16-bit**, so GPU memory and bandwidth are unchanged (M8 REPORT).
- PVRTC 4bpp is native to the hardware and stays compressed (`glCompressedTexImage2D` at `0x08483794`, format table `0x0874f14c`). It gives 4× less memory and fetch bandwidth than 16bpp, and makes the full-game texture budget of about 8 KB per 128² texture achievable.
- Requirements and risks:
  - Square, power-of-two textures.
  - Visible artifacts on alpha and on UI.
  - NanoEmu has no PVRTC decoder, so `load_tex()` in `nano7_mbxr.c` needs one before it can be validated.
- Keep 16bpp RGBA4444 for the UI and fonts.

### P10: CPU micro-optimisations (after profiling)

- **IRAM:** copy proven hot loops into the ~48 KB unused IRAM window at `0x22020000–0x2202c000`: collision triangle tests, the skinning inner loop, the mixer. First verify that the window really is free while ViceNano runs.
- **L1 prefetch:** set ACTLR L1PCTL. This needs a privileged context.
- **Governor:** lock the P-state so it doesn't step down mid-game.
- Expected 5–15% on the targeted loops only.

### P11: clocks

- **The harness must reach 30 FPS at stock clocks.** It is latency-bound, so clocks don't help it.
- For the full game:
  - CPU at 684 MHz is the main lever, but no evidence shows it is stable on this silicon (`overclock-limits.md`).
  - GPU at 171 MHz is the highest entry in RetailOS's own timing table. The GPU is 8% busy, so don't use the 266 MHz poke.
  - Watch temperature: the LiPo is glued against the panel.

## 4. Path to 30 FPS

1. **P0 + P1 + P2 (about a week):** the harness should go from ~20 to **30+ in NanoEmu at stock clocks**, because the frame work falls from ~20+ ms to under 8 ms. If the P0 micro-benchmark shows `glDrawArrays` also costs about 1 ms per draw, the diagnosis is wrong. In that case, bisect the frame with the new timers before doing anything else.
2. **P3 + P4:** lock 30 with no 3-vsync frames in the histogram (gate: ≥ 99% of frames take ≤ 2 vsync intervals over a 160 s run). This gives the M8 gate a real pass and makes it a stricter 30 FPS gate than the current 24+.
3. **Device check (M9 prerequisite):** run the same binary on hardware and repeat the micro-benchmark and the vertex-budget ramp.
4. **Full game:** P5, P6 and P7 decide whether 30 FPS holds once reVC simulation, peds, cars and real textures arrive. P8, P9 and P10 add headroom. Expect suburbs and interiors at 30. Dense downtown driving is the risk case (estimated 22–30 FPS), and P6 (draw distance and LOD) and P7 (population) are the dials for it.

## 5. Evidence index

- `m8/REPORT.md`, `m8/OPTIMIZATION_REPORT.md`, `m8/optimization-runs/01-max-baseline/{capabilities.json,emulator.log}`
- `ViceNano/sdk/gl.c:332-376` (blocking indexed draw), `m8/model.cpp:251-286` (batch loop), `m8/viewer.cpp` (overlay), `m8/nano.cpp` (frame), `m8/Makefile` (nofp libm)
- `nano7-untethered/nanoapps/system/silver_resident/silver_resident.c:407,516-531,876-886` (16 ms heartbeat → Inval → GL frame)
- `nano7-untethered/nanoapps/apps/mk64/mk64_entry.c:224-246` (30 FPS replay pacing), `games/README.md:32` (16–20 ms frames)
- `NanoEmu/docs/research/{gpu-deep-dive,display-cap,cpu-deep-dive}.md`, `NanoRework/docs/research/{overclock-limits,mk64-renderer-tricks,neon-vfp}.md`
- `ViceNano/docs/{vcphys.md,agy-vcfps.txt,texture-budget.md}`
