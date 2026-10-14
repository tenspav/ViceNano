<p align="center">
  <img src="docs/logo-256.png" width="192" alt="ViceNano logo">
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

## Development progress

| Stage | Progress | Status |
|---|---|---|
| 🔬 Research | ![100%](https://img.shields.io/badge/100%25-brightgreen) | every chip documented |
| 🖥️ Emulator | ![93%](https://img.shields.io/badge/93%25-brightgreen) | boots real OS, live window, audio landing |
| 🔀 Dualboot | ![40%](https://img.shields.io/badge/40%25-yellow) | chainload path validated |
| 🎮 GPU access | ![30%](https://img.shields.io/badge/30%25-yellow) | mapped, draft QEMU model written |
| 🏝️ Engine port | ![15%](https://img.shields.io/badge/15%25-orange) | reVC → GLES1; proven path: [nano7-untethered](https://github.com/olievans123/nano7-untethered) runs Mario Kart 64 natively at 30 fps with a working GLES renderer |
| 🕹️ Controls | ![50%](https://img.shields.io/badge/50%25-yellowgreen) | BT classic confirmed, HID plan ready |

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

## Research feeding this project

All hardware documentation lives in [NanoEmu/docs/research](https://github.com/tenspav/NanoEmu/tree/main/docs/research): GPU/CPU clock unlocks, NAND streaming measurements, the Bluetooth classic stack, the 40 MB shared heap finding, and the dualboot analysis.

## Legal

reVC is a reverse-engineered clean-room reimplementation; game assets are never distributed. You need your own copy of GTA Vice City (PC 1.0). Not affiliated with Rockstar Games, Take-Two, or Apple.
