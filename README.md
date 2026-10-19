<p align="center">
  <img src="assets/logo-256.png" width="192" alt="ViceNano logo">
</p>

<h1 align="center">ViceNano</h1>

<p align="center">
  <b>Grand Theft Auto: Vice City running on an iPod nano 7G (2012)</b><br>
  Built on <a href="https://github.com/tenspav/NanoEmu">NanoEmu</a> + the reversed <a href="https://github.com/mrxenginner/reVC">reVC</a> engine
</p>

<p align="center">
  <img alt="status" src="https://img.shields.io/badge/status-hardware%20bring--up-yellow">
  <img alt="target" src="https://img.shields.io/badge/target-iPod%20nano%207G-lightgrey">
  <img alt="engine" src="https://img.shields.io/badge/engine-reVC-orange">
</p>

---

## Why it can work

| Requirement | PS2 (2002) | nano 7G (unlocked) |
|---|---|---|
| 🧠 CPU | 300 MHz | **684 MHz** (proven unlockable from 342) |
| 💾 RAM | 32 MB | **64 MB** |
| 🎮 GPU | GS ~147 MHz | PowerVR MBX Lite, 133 MHz stock → **171-266 MHz** unlockable |
| 💿 Streaming | DVD drive | NAND flash, **~25-35 MB/s** measured |

> **Hardware milestone (2026-10-19):** the M8 world preview now runs continuously on the real iPod nano 7G GPU at about **70 fps** with MMU + D-cache enabled: 3,121 frames, 421,779 triangles, 0 GPU errors in the on-device report (R80). The earlier parameter-pool stall (scene 11) is fixed. The scene is playable with a **paired DualSense gamepad** (Bluetooth HID on the device) or touch camera control.
>
> **Engine milestone (2026-10-19, reVC stage 2):** real, unmodified reVC source files (skeleton, CTimer, math, CSprite2d, RenderBuffer) now run inside NanoOS through the NanoOSLoader game ABI: an NPAK v2 asset pack is mounted and CRC-verified, the MBX GPU renders through the kernel driver (with CPU fallback), and input maps to reVC's real controller layer. ~45 fps in the emulator, clean Home exit, stage-1 loader compatibility preserved. Next: stage 3, conversion of the real GTA Vice City PC assets.

> **Measured performance (emulator timing model):** ~19.97 FPS at stock clocks, ~20.56 FPS average unlocked (best fresh run peaked at 23.25 FPS but did not sustain 24+). The 24+ FPS target needs further optimizer work or real-device overclock headroom. Streaming worst case (Downtown, 15.9 MB) fits the 18 MB pool. Details: [`m8/REPORT.md`](m8/REPORT.md), [`m8/OPTIMIZATION_REPORT.md`](m8/OPTIMIZATION_REPORT.md).

## Development progress

> **Renderer progress:** [M1b](m1b/REPORT.md) verifies native indexed draws; [M2](m2/REPORT.md) renders through librw GLES1; [M3](m3/REPORT.md) renders a VC cone; [M4](m4/REPORT.md) renders a building through reduced reVC. [M5](m5/REPORT.md) streams three real IPL/IDE building cells with a two-block LRU: 66 loads, 64 evictions, 120-byte unloaded-heap variation, and distinct QMP views in NanoEmu, below the 18 MiB budget. [M6](m6/REPORT.md) adds DMA audio: six concurrent SFX voices observed, a complete 60-second radio stream verified in capture, and 28 stop/reinitialise cycles with 100-byte heap variation. [M7](m7/REPORT.md) integrates player input: on-screen touch controls (virtual stick, camera swipe, 10 buttons) and encrypted Bluetooth HID gamepad drive player marker movement and camera orbit in the streamed world; 542 touch events verified with 0-byte heap growth, 7 secure HID reports and full-screen overlay suppression verified in NanoEmu. Packs and compressed radio backing are embedded; NAND streaming, device and full-game gates remain open.



| Stage | Progress | Status |
|---|---|---|
| 🔬 Research | ![100%](https://img.shields.io/badge/100%25-brightgreen) | every chip documented |
| 🖥️ Emulator | ![98%](https://img.shields.io/badge/98%25-brightgreen) | boots real OS, window, audio, 3D games, save states, HUD, clock profiles |
| 🔀 Dualboot | ![75%](https://img.shields.io/badge/75%25-yellowgreen) | chainloader round trip works in NanoEmu (OS -> bare metal -> OS) |
| 🎮 GPU access | ![95%](https://img.shields.io/badge/95%25-brightgreen) | full NanoGL renderer with VGP interpreter in the emulator; **real MBX GPU runs the M8 world preview continuously on hardware (~70 fps, pool leak fixed)**; PVRTC pending |
| 🏝️ Engine port | ![82%](https://img.shields.io/badge/82%25-green) | reVC → GLES1; [execution plan M0-M9](docs/exec-plan.md); **reVC stage 2: real reVC code runs on NanoOS via the game loader (~45 fps in emulator)**; [nano7-untethered](https://github.com/olievans123/nano7-untethered) proves 30 fps 3D on this device |
| 🕹️ Controls | ![100%](https://img.shields.io/badge/100%25-brightgreen) | touch + BT classic HID host verified; **DualSense pairs and plays on real hardware** |

### Engine port milestones (M0-M9)

| Milestone | Progress | What it proves |
|---|---|---|
| M0 triangle | ![done](https://img.shields.io/badge/done-brightgreen) | first pixel through NanoGL |
| M1 cube | ![done](https://img.shields.io/badge/done-brightgreen) | textured 3D, native indexed draws |
| M2 librw | ![done](https://img.shields.io/badge/done-brightgreen) | reVC's graphics library runs on NanoEmu |
| M3 VC model | ![done](https://img.shields.io/badge/done-brightgreen) | real Vice City asset renders ([cone](m3/REPORT.md)) |
| M4 reVC core | ![done](https://img.shields.io/badge/done-brightgreen) | reduced reVC viewer renders a VC building ([m4](m4/REPORT.md)) |
|  M5 world stream| ![done](https://img.shields.io/badge/done-brightgreen) | three building cells stream below 18 MiB; LRU and 64 evictions verified ([m5](m5/REPORT.md)) |
| M6 audio | ![done](https://img.shields.io/badge/done-brightgreen) | SFX + 60-second radio through DMA/I2S; capture, timing and 28 cycles verified ([m6](m6/REPORT.md)) |
| M7 controls | ![done](https://img.shields.io/badge/done-brightgreen) | touch + BT gamepad drive the game ([m7](m7/REPORT.md)) |
| M8 performance | ![done](https://img.shields.io/badge/done-brightgreen) | code complete; 24+ FPS target not sustained (~20.56 FPS unlocked, ~19.97 FPS stock) |
| M8 on hardware | ![done](https://img.shields.io/badge/done-brightgreen) | triangle, textured quad and the continuous world preview render on the real MBX GPU (~70 fps, 0 errors); DualSense + touch control |
| reVC stage 2 | ![done](https://img.shields.io/badge/done-brightgreen) | real reVC code runs on NanoOS via the game loader (NPAK assets, MBX rendering, input), ~45 fps in emulator |
| reVC stage 3 | ![in progress](https://img.shields.io/badge/in%20progress-yellow) | GTA Vice City PC asset conversion (IMG/TXD/DFF) into NPAK packs |
| M9 device | ![planned](https://img.shields.io/badge/planned-lightgrey) | full game on the real iPod via dualboot |

**Overall: research 100% + emulator 98% + engine 82%. M8 runs continuously on the real iPod GPU at ~70 fps, and the real reVC engine code boots inside NanoOS (stage 2). Ahead: stage 3 asset conversion, then the full game on the device.**

## The stages

0. ✅ **3D on real hardware is proven**: [nano7-untethered](https://github.com/olievans123/nano7-untethered) ships native Mario Kart 64 (30 fps) and Super Mario 64 ports with an OpenGL ES renderer + tilt steering on this exact device
1. ✅ **Map the hardware**: done in NanoEmu, every chip documented
2. ✅ **Emulator as testbed**: NanoEmu boots the real RetailOS with a live window
3. ✅ **Emulated GPU renders real 3D**: full NanoGL renderer with VGP interpreter
4. ✅ **reVC builds for ARM**: reduced reVC viewer runs in NanoEmu (M4)
5. ✅ **GLES1 backend for librw**: verified through M2-M5
6. 🔜 **Chainloader**: plan validated, payload + hardware shutdown pieces built
7. ⬜ **ViceNano boots in the emulator**: full RAM, unlocked clocks, world streams from NAND
8. ✅ **ViceNano on real hardware**: M8 scenes run on the real MBX GPU (~70 fps, DualSense playable); dualboot via NanoOS menu works on device (StockOS returns to the original OS)
9. ⬜ **Mini handheld**: 3D-printed cradle + 8BitDo Lite 2 gamepad (BT HID pairing already works in NanoEmu) + battery

Asset math is closed: the whole game fits via PVRTC 4bpp textures (39 MB) or 8bpp palettes (147 MB), with 1,000-3,000 concurrent textures in the streaming window ([`docs/texture-budget.md`](docs/texture-budget.md)).

### Hardware bring-up tooling

- RSN failure codes identify why a GPU frame failed.
- Hardware reports (`R80.txt`) are auto-saved to the iPod disk after each run.
- A buttonless auto-reboot test loop runs scenes repeatedly without touching the device.

### Asset extraction

Vice City PC 1.0 extraction is mapped (6043 IMG entries). The first building and road are converted to `.nantex` and validated.

## Research feeding this project

All hardware documentation lives in [NanoEmu/docs/research](https://github.com/tenspav/NanoEmu/tree/main/docs/research): GPU/CPU clock unlocks, NAND streaming measurements, the Bluetooth classic stack, the 40 MB shared heap finding, and the dualboot analysis.

## Legal

reVC is a reverse-engineered clean-room reimplementation; game assets are never distributed. You need your own copy of GTA Vice City (PC 1.0). Not affiliated with Rockstar Games, Take-Two, or Apple.
