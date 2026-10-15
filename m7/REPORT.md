# M7 — controls/input

**PASS for touchscreen on-screen controls and Bluetooth HID gamepad integration in the reduced reVC build in NanoEmu.** Scope follows the requested M7 controls milestone and README; the older execution-plan table numbers world visibility as M7 and gameplay controls as M8. The same ARM package renders M5's two-slot streamed city through retained reVC fakerw/clump/texture helpers while player input dynamically drives the player marker and following camera. No physical device, SDK or reference reVC tree was modified. No commit was made.

## Implementation

[`input.cpp`](input.cpp) and [`input.h`](input.h) provide a reduced `CPad` adapter preserving 20 core controller fields, pressure states, and DualShock/reVC axis semantics. Source verification ([`source-check.json`](source-check.json)) verifies all 20 retained field names and `int16_t` types against the read-only reference `Pad.h`. The adapter provides:
- **On-Screen Touch Pipeline (Scheme A / 240x432 native, 432x240 landscape):**
  - Left floating analog stick ([X: 10..130, Y: 70..235], radius 42 px) mapping dynamic drag displacement to `LeftStickX` and `LeftStickY` (±32767).
  - Central swipe zone ([X: 140..310, Y: 40..200]) feeding camera yaw and pitch via `RightStickX` and `RightStickY`.
  - Ten discrete touch hitboxes matching [`docs/m7-controls-map.md`](../docs/m7-controls-map.md): Start (Pause), RightShock (Look Back), LeftShoulder2 (Weapon Prev), RightShoulder2 (Weapon Next), RightShoulder1 (Aim/Lock), Triangle (Enter), Circle (Attack), Square (Jump), Cross (Sprint/Run), and LeftShock (Crouch).
  - Queue phase decoding for RetailOS digitizer coordinates (0 down, 2 move, 1 up), with multi-finger latching, resident-latched touch mailbox support, and an abandoned-queue release watchdog bounding unobserved releases.
- **Bluetooth HID Gamepad Pipeline (Scheme B / hidhost):**
  - Uses the unchanged Session 51 Bluetooth HID host layer ([`hh_bt.c`](hh_bt.c), [`hh_bt.h`](hh_bt.h)) and streaming HID descriptor parser ([`hid_parse.c`](hid_parse.c), [`hid_parse.h`](hid_parse.h)).
  - Initiates outgoing connection to the default known peer (NanoPad `02:4e:50:41:44:01`), executing SDP `ServiceSearchAttribute` for `HIDDescriptorList` (UUID `0x1124`), dynamic descriptor parsing, Just Works SSP user confirmation reply, link encryption, and dual L2CAP channels (PSM `0x0011` control and PSM `0x0013` interrupt).
  - Maps 16 digital buttons, analog axes (X, Y, Z, Rz, Rx, Ry), and hat switch into `CPad::NewState`. NanoPad Z/Rz axes are normalized to right-stick Rx/Ry.
  - When `hh_bt_state() == HH_CONNECTED`, all on-screen virtual controls are suppressed and hidden, giving clean full-screen 3D rendering and prioritizing gamepad input.
- **World & Camera Movement Integration ([`nano.cpp`](nano.cpp)):**
  - Input directly controls the yellow diamond player marker in world space within the streamed North Beach IPL cells.
  - Cross button doubles movement speed (run/sprint). Camera smoothly orbits with yaw and pitch clamped to [-30°, +40°], continuously centering the player marker.

## Verification

[Host checks](host-check.log) pass with ASan/UBSan: simultaneous stick/button/camera updates, release handling, all 10 on-screen buttons, HID axis/button/hat mapping, disconnect clearing, and 3,000 rapid touch state updates.

[Package checks](package-check.log) pass HRL1 relocation and bounds checks (1,414 relocations, 9 packed relative constructors). The ARM ELF has zero undefined symbols ([`undefined-symbols.log`](undefined-symbols.log)). The Thumb/soft-float gate passes. The final executable is 729,112 bytes with an image span of 772,552 bytes and 49,120 BSS bytes, fitting well inside the Mario Kart slot.

[NanoEmu QMP verification](pass/capabilities.json) verifies all 14 gates across touch and Bluetooth gamepad operation:
- **Touch Input & Heap Stability:**
  - 542 guest-observed touch events measured (threshold: 500+).
  - Zero heap growth across the touch event sequence: warm OS free heap is 6,051,692 B before and after (0 B difference). Tracked live RW bytes remain constant at 411,980 B (peak 414,108 B, within the 18 MiB envelope).
  - Touch dragging advances the player marker in world coordinates (`player_x16` moves from 7960 to 8847).
  - Touch swiping rotates camera yaw and tilts pitch.
- **Bluetooth Pairing & Secure HID:**
  - On-screen Start button triggers outgoing Bluetooth stack power-on.
  - NanoEmu logs confirm fresh SSP pairing with "NanoPad" (link key type 4), link encryption enabled, SDP descriptor retrieval (62 bytes), and both L2CAP PSM 0x11 and PSM 0x13 opened.
  - All 7 scripted HID reports decoded and verified: neutral initial, left stick deflection (`lx=100`) driving player movement, right stick deflection (`rx=100`) orbiting the camera to yaw 5174, and button `a` (Cross) sprinting with `ly=-100`.
  - On gamepad connection (`bt_state == 4`), all on-screen buttons and virtual stick are hidden.
- **Scene and Renderer Health:**
  - Framebuffer diff detects 18,006 changed pixels between touch and baseline scenes.
  - Zero guest errors and zero allocation failures across all 24 sample frames.
  - Disk integrity check ([`pass/check.json`](pass/check.json)) confirms host disk size, mtime, and all replaced FTL blocks remain unmodified.

| Metric | Measured Value | Requirement | Status |
|---|---:|---:|:---:|
| Observed touch events | 542 | ≥ 500 | PASS |
| OS free heap delta over 500+ events | 0 B | ≤ 512 B | PASS |
| Live RW heap allocation | 411,980 B | ≤ 18 MiB | PASS |
| RW allocation peak | 414,108 B | ≤ 18 MiB | PASS |
| Touch player marker movement (ΔX, ΔZ) | +55.4 world units | > 1.0 unit | PASS |
| Touch camera yaw rotation | +8.56° | > 1.0° | PASS |
| HID reports received | 7 | ≥ 7 | PASS |
| Gamepad link state | 4 (Connected) | 4 | PASS |
| HID marker displacement | +3.1 world units | > 1.0 unit | PASS |
| HID camera yaw rotation | +43.18° | > 1.0° | PASS |
| Guest GL / allocation errors | 0 | 0 | PASS |
| Scene changed pixels | 18,006 px | > 1,000 px | PASS |

Package SHA256: `13a54d8a75238c567ccc0e8fefab97fe80d3ebc08d443d84b96f0f67d59d70b9`.
Representative screenshots: [`pass/touch-moved.png`](pass/touch-moved.png), [`pass/touch-camera.png`](pass/touch-camera.png), and [`pass/vcinput-7.png`](pass/vcinput-7.png).
Reproduction instructions are documented in [`README.md`](README.md).
