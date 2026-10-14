# ViceNano

**Grand Theft Auto: Vice City running on an iPod nano 7G (2012).** A homebrew porting project built on top of [NanoEmu](https://github.com/tenspav/NanoEmu) and the [reVC](https://github.com/mrxenginner/reVC) reversed engine.

## Why it can work

| Requirement | PS2 (2002) | nano 7G (unlocked) |
|---|---|---|
| CPU | 300 MHz | 684 MHz (proven unlockable from 342) |
| RAM | 32 MB | 64 MB |
| GPU | GS ~147 MHz | PowerVR MBX Lite, 133 MHz stock, 171-266 MHz unlockable |
| Storage streaming | DVD | NAND flash, ~25-35 MB/s measured |

## Development progress

```
Research     ████████████████████ 100%   hardware fully mapped
Emulator     ███████████████████░  93%   boots real OS, live window, audio in progress
Dualboot     ████████░░░░░░░░░░░░  40%   chainload path validated
GPU access   ██████░░░░░░░░░░░░░░  30%   mapped + draft QEMU model
Engine port  ░░░░░░░░░░░░░░░░░░░░   0%   reVC -> OpenGL ES 1.1 (librw backend)
Controls     ██████████░░░░░░░░░░  50%   BT classic stack confirmed, HID plan ready
```

## The stages

1. ✅ **Map the hardware** (done in NanoEmu: every chip documented)
2. ✅ **Emulator as testbed** (NanoEmu boots the real RetailOS with a live window)
3. 🔜 **Emulated GPU renders real 3D** (MBX model exists as draft, needs real rendering)
4. ⬜ **reVC builds for ARM Linux** (first proof on a PC, no iPod involved)
5. ⬜ **GLES1 backend for librw** (the GPU speaks OpenGL ES 1.1 fixed pipeline)
6. ⬜ **Chainloader** (homebrew app that hands the device to a bare-metal game OS with all 64 MB)
7. ⬜ **ViceNano boots on the emulator** (full RAM, unlocked clocks, game streams from NAND)
8. ⬜ **ViceNano on real hardware** (dualboot: hold buttons at boot, original OS untouched)
9. ⬜ **Mini handheld** (3D-printed case + Bluetooth gamepad + battery)

## Research feeding this project

All hardware documentation lives in [NanoEmu/docs/research](https://github.com/tenspav/NanoEmu/tree/main/docs/research): GPU/CPU clock unlocks, NAND streaming measurements, Bluetooth classic stack, the 40 MB shared heap finding, and the dualboot analysis.

## Legal

reVC is a reverse-engineered clean-room reimplementation; game assets are never distributed. You need your own copy of GTA Vice City (PC 1.0). Not affiliated with Rockstar Games, Take-Two, or Apple.
