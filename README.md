<p align="center">
  <img src="assets/logo-256.png" width="192" alt="ViceNano logo">
</p>

<h1 align="center">ViceNano</h1>

<p align="center">
  <b>Grand Theft Auto: Vice City running on an iPod nano 7G (2012)</b><br>
  Built on <a href="https://github.com/tenspav/NanoEmu">NanoEmu</a> + the reversed <a href="https://github.com/mrxenginner/reVC">reVC</a> engine
</p>

<p align="center">
  <img alt="status" src="https://img.shields.io/badge/status-research%20complete-brightgreen">
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

> **Performance estimate is in:** ~16 FPS at stock clocks (marginal), **24-30 FPS fully playable with the unlocked 684 MHz CPU + NEON skinning + batching**. Streaming worst case (Downtown, 15.9 MB) fits the 18 MB pool. Details: [`docs/vcfps.md`](docs/vcfps.md), [`docs/vcstream.md`](docs/vcstream.md).

## Development progress

> **Renderer progress:** [M1b](m1b/REPORT.md) verifies native indexed draws; [M2](m2/REPORT.md) renders through librw GLES1; [M3](m3/REPORT.md) renders a VC cone; [M4](m4/REPORT.md) renders a building through reduced reVC. [M5](m5/REPORT.md) streams three real IPL/IDE building cells with a two-block LRU: 66 loads, 64 evictions, 120-byte unloaded-heap variation, and distinct QMP views in NanoEmu, below the 18 MiB budget. Packs are embedded; NAND streaming, device and full-game gates remain open.



| Stage | Progress | Status |
|---|---|---|
| 🔬 Research | ![100%](https://img.shields.io/badge/100%25-brightgreen) | every chip documented |
| 🖥️ Emulator | ![97%](https://img.shields.io/badge/97%25-brightgreen) | boots real OS, window, audio, **renders 3D games** |
| 🔀 Dualboot | ![40%](https://img.shields.io/badge/40%25-yellow) | chainload path validated |
| 🎮 GPU access | ![65%](https://img.shields.io/badge/65%25-yellowgreen) | MBX renders real 3D in NanoEmu v1.0.0 |
| 🏝️ Engine port | ![60%](https://img.shields.io/badge/60%25-green) | reVC → GLES1; [execution plan M0-M9](docs/exec-plan.md) done; [nano7-untethered](https://github.com/olievans123/nano7-untethered) proves 30 fps 3D on this device |
| 🕹️ Controls | ![50%](https://img.shields.io/badge/50%25-yellowgreen) | BT classic confirmed, HID plan ready |

### Engine port milestones (M0-M9)

| Milestone | Progress | What it proves |
|---|---|---|
| M0 triangle | ![done](https://img.shields.io/badge/done-brightgreen) | first pixel through NanoGL |
| M1 cube | ![done](https://img.shields.io/badge/done-brightgreen) | textured 3D, native indexed draws |
| M2 librw | ![done](https://img.shields.io/badge/done-brightgreen) | reVC's graphics library runs on NanoEmu |
| M3 VC model | ![done](https://img.shields.io/badge/done-brightgreen) | real Vice City asset renders ([cone](m3/REPORT.md)) |
| M4 reVC core | ![done](https://img.shields.io/badge/done-brightgreen) | reduced reVC viewer renders a VC building ([m4](m4/REPORT.md)) |
|  M5 world stream| ![done](https://img.shields.io/badge/done-brightgreen) | three building cells stream below 18 MiB; LRU and 64 evictions verified ([m5](m5/REPORT.md)) |
| M6 audio | ![planned](https://img.shields.io/badge/planned-lightgrey) | SFX + radio through our audio path |
| M7 controls | ![planned](https://img.shields.io/badge/planned-lightgrey) | touch + BT gamepad drive the game |
| M8 performance | ![planned](https://img.shields.io/badge/planned-lightgrey) | 24+ fps sustained with unlocked clocks |
| M9 device | ![planned](https://img.shields.io/badge/planned-lightgrey) | runs on the real iPod via dualboot |

**Overall: research 100% + emulator 97% + engine 50% = the game itself is the remaining work.**

## The stages

0. ✅ **3D on real hardware is proven** — [nano7-untethered](https://github.com/olievans123/nano7-untethered) ships native Mario Kart 64 (30 fps) and Super Mario 64 ports with an OpenGL ES renderer + tilt steering on this exact device
1. ✅ **Map the hardware** — done in NanoEmu, every chip documented
2. ✅ **Emulator as testbed** — NanoEmu boots the real RetailOS with a live window
3. 🔜 **Emulated GPU renders real 3D** — MBX model drafted, needs real rendering
4. ⬜ **reVC builds for ARM Linux** — first proof on a PC, no iPod involved
5. ⬜ **GLES1 backend for librw** — the GPU speaks OpenGL ES 1.1 fixed pipeline
6. ⬜ **Chainloader** — homebrew app hands the device to a bare-metal game OS with all 64 MB
7. ⬜ **ViceNano boots in the emulator** — full RAM, unlocked clocks, world streams from NAND
8. ⬜ **ViceNano on real hardware** — dualboot: hold buttons at boot, original OS untouched
9. ⬜ **Mini handheld** — 3D-printed case + Bluetooth gamepad + battery

Asset math is closed: the whole game fits via PVRTC 4bpp textures (39 MB) or 8bpp palettes (147 MB), with 1,000-3,000 concurrent textures in the streaming window ([`docs/texture-budget.md`](docs/texture-budget.md)).

## Research feeding this project

All hardware documentation lives in [NanoEmu/docs/research](https://github.com/tenspav/NanoEmu/tree/main/docs/research): GPU/CPU clock unlocks, NAND streaming measurements, the Bluetooth classic stack, the 40 MB shared heap finding, and the dualboot analysis.

## Legal

reVC is a reverse-engineered clean-room reimplementation; game assets are never distributed. You need your own copy of GTA Vice City (PC 1.0). Not affiliated with Rockstar Games, Take-Two, or Apple.
