# ViceNano (reVC on iPod nano 7G) Milestone M7 Controls Architecture & Mapping
# Target: S5L8747 SoC | 240x432 (Landscape 432x240) | Multi-touch SSP2 | Physical Buttons | LIS3DH Accel | BT HID Host

================================================================================
1. EXECUTIVE SUMMARY & REVC INPUT SUBSYSTEM ARCHITECTURE
================================================================================
Vice City gameplay requires simultaneous 2D character/vehicle navigation (Move stick),
free camera/turret manipulation (Camera stick), and 10 discrete gameplay actions:
  (1) Attack / Fire / Melee       (6) Look Back / Glance Left & Right
  (2) Jump / Brake / Reverse      (7) Radio Station Cycle
  (3) Sprint / Accelerate (Gas)   (8) Weapon Cycle (Next / Prev)
  (4) Enter / Exit Vehicle        (9) Aim / Lock-on Target / Sniper Zoom
  (5) Handbrake / Crouch (Duck)  (10) Horn / Siren / Submissions (Taxi/Vigilante)

reVC Input Engine Hook (src/core/Pad.h, Pad.cpp, ControllerConfig.h):
  - High-level engine polls CPad::GetPad(0)->UpdatePads() once per frame.
  - State is stored in CControllerState (DualShock layout, int16 values):
      LeftStickX, LeftStickY       : [-32767 .. 32767] (Walk/Steer)
      RightStickX, RightStickY     : [-32767 .. 32767] (Camera/Aim/Look)
      Cross, Square, Circle, Triangle: [0, 255] (Sprint/Gas, Jump/Brake, Attack, Enter)
      LeftShoulder1, RightShoulder1: [0, 255] (Radio/Crouch, Target Lock/Handbrake)
      LeftShoulder2, RightShoulder2: [0, 255] (Look L/Wpn Prev, Look R/Wpn Next)
      DPadUp, DPadDown, DPadLeft, DPadRight: [0, 255] (Radio/Menu nav)
      Start, Select                : [0, 255] (Pause Menu, Cycle Camera View)
      LeftShock, RightShock        : [0, 255] (L3 Horn/Crouch, R3 Look Behind)
  - The engine contextually switches action semantics between ON-FOOT and IN-VEHICLE
    modes based on CWorld::Players[0].m_pPed->bInVehicle.

Available Hardware Resources on iPod nano 7G:
  - Multi-touch Digitizer: SSP2 SPI (0x3d200000), GPIO 0x26 IRQ 32, up to 8 contacts,
    native 240x432 digitizer mapped to landscape 432x240 display.
  - Physical Buttons (5 discrete inputs):
    * Vol Up    : GPIO 0x28 (bank 4, pin 8), active-low level at 0x3cf000a4 bit 0.
    * Vol Down  : GPIO 0x29 (bank 4, pin 9), active-low level at 0x3cf000a4 bit 1.
    * Play/Pause: PMU register 3 bit 3 (center pill between volume buttons).
    * Sleep/Wake: PMU register 3 bit 1 / register 0x07 bit 4 (top-right edge).
    * Home Chin : PMU register 3 bit 0 / register 0x07 bit 5 (front bezel chin).
  - Accelerometer: LIS3DH 3-axis I2C1 slave 0x18, 100Hz hardware sampling via 0x08a91e50.
  - Bluetooth Classic Controller: BCM2076B1 via high-speed UART 0x3db00000 at 2.4 MBaud;
    L2CAP PSM 0x0013 HID host stack (probe/hidhost) with SDP UUID 0x1124 and SSP pairing.

================================================================================
2. SCHEME (A): TOUCH-ONLY ON-SCREEN LAYOUT (LANDSCAPE 432x240)
================================================================================
Designed for pure handheld touch operation without touching hardware buttons.
Screen is divided into ergonomic thumb zones: Left Thumb [0..130px], Center Clear
Corridor [131..301px] (170px width preserved for 3D action view & subtitles), and
Right Thumb [302..432px].

[A.1] ON-FOOT TOUCH LAYOUT (432x240)
+--------------------------------- 432 px ----------------------------------+
|[Pause]  [LookBack]                       [Weapon/Ammo] [HP/Arm] [Aim/Lock]| 0
| (0,0)    (50,0)                            (310,5)     (365,5)   (395,0)  |
|                                                                           |
|                                                                 [Enter]   |
|   +---------------+                                             (375,65)  |
|   | Floating Stick|    [Subtitles / Dialogue]            [Attack]         |
|   |  (Move/Walk)  |                                      (325,120) [Sprint]|
|   |  r=42px zone  |                                                (375,130)|
|   +---------------+                                         [Jump]        |
|    [Crouch]                 [Minimap/Radar]                 (325,180)     |
|    (15,195)                     (150,175)                                 |
+---------------------------------------------------------------------------+ 240
  0 px                                                               432 px

On-Foot Control Mappings & Touch Hitboxes:
  - Move Stick    : Dynamic floating stick spawned on touch-down in [X: 10..130, Y: 70..235].
                    Drag vector (dx, dy) normalized to radius R=42px; maps directly to
                    LeftStickX = clamp(dx*780, -32767, 32767), LeftStickY = clamp(-dy*780).
  - Camera Look   : Open swipe zone in center-right screen [X: 140..310, Y: 40..200].
                    Touch deltas (dx, dy) feed RightStickX / RightStickY or Mouse look.
  - Attack / Fire : Circle button [X: 310..365, Y: 105..160] (r=27px) -> CControllerState::Circle.
  - Jump          : Square button [X: 310..365, Y: 165..220] (r=27px) -> CControllerState::Square.
  - Sprint        : Cross button  [X: 365..425, Y: 120..185] (r=30px) -> CControllerState::Cross.
  - Enter Vehicle : Triangle icon [X: 365..420, Y: 55..110]  (r=25px) -> CControllerState::Triangle.
                    Contextual opacity: 25% when no car nearby; 90% pulsing when near door.
  - Aim / Lock-on : Top-right tab [X: 385..430, Y: 0..45]    -> CControllerState::RightShoulder1.
  - Crouch / Duck : Bottom-left pill [X: 10..55, Y: 190..235] -> CControllerState::LeftShock (L3).
                    Alternate gesture: Double-tap center of Move Stick.
  - Weapon Cycle  : Weapon HUD icon [X: 305..360, Y: 0..40].
                    Tap right half -> CycleWeaponRight (R2); tap left half -> CycleWeaponLeft (L2);
                    or horizontal swipe gesture across the icon.
  - Look Back     : Top-left pill [X: 45..95, Y: 0..40]       -> CControllerState::RightShock (R3).
  - Pause / Menu  : Top-left icon [X: 0..40, Y: 0..40]        -> CControllerState::Start.

[A.2] IN-VEHICLE TOUCH LAYOUT (432x240)
+--------------------------------- 432 px ----------------------------------+
|[Pause]  [RadioBar]                       [Vehicle Name] [Damage] [ExitCar]| 0
| (0,0)   (50,0..110)                                              (380,5)  |
|                                                                           |
| [LookL]   [LookR]                                              [DriveBy]  |
| (10,55)   (65,55)                                              (325,65)   |
|                                                                           |
|   +---------------+                                      [Handbrake]      |
|   | Steer Left /  |    [Subtitles / Radio Title]          (315,120) [Gas] |
|   |  Steer Right  |                                                (370,120)|
|   +---------------+                                         [Brake]       |
|    [Horn/Siren]             [Minimap/Radar]                 (315,180)     |
|    (15,195)                     (150,175)                                 |
+---------------------------------------------------------------------------+ 240

In-Vehicle Control Mappings & Touch Hitboxes:
  - Steer Left/Right: Left horizontal analog zone [X: 10..125, Y: 95..190]. Center deadzone
                      ±8px; horizontal displacement maps to LeftStickX [-32767 .. 32767].
  - Accelerate (Gas): Large vertical pedal [X: 365..425, Y: 110..195] -> Cross.
  - Brake / Reverse : Large vertical pedal [X: 305..360, Y: 155..235] -> Square.
  - Handbrake       : Oval button above brake [X: 305..360, Y: 95..150] -> RightShoulder1 (R1).
  - Exit Vehicle    : Top-right icon [X: 375..430, Y: 0..50]           -> Triangle.
  - Drive-by Fire   : Combat button [X: 310..365, Y: 45..90]           -> Circle.
  - Look Left/Right : Side glance tabs [X: 10..55, Y: 50..90] (L2) and [X: 60..105, Y: 50..90] (R2).
                      Pressing both simultaneous tabs triggers Look Behind (L2 + R2).
  - Horn / Siren    : Button [X: 10..55, Y: 190..235] -> LeftShock (L3). Tap to honk/siren;
                      hold 1.0s to trigger Taxi / Vigilante / Paramedic sub-missions.
  - Radio Station   : Top bar [X: 45..120, Y: 0..35]. Swipe right for Next Station, swipe left
                      for Prev Station; tap to toggle Radio Off.

Technical Implementation Notes for Scheme (A):
  - Rendering: 2D touch HUD rendered via librw Im2D / GLES1 textured quads in RGBA4444.
  - Touch Pipeline: SSP2 interrupt dispatches packets; MultitouchTask fills queues at
    0x089a5298 + 24*n. Coordinate transform:
      display_x = 431 - trunc((raw_y + 75) * 431 / 5250)
      display_y = trunc((raw_x + 75) * 239 / 2845)
  - Multi-touch Concurrency: Supports up to 3 simultaneous active contacts (e.g., Left Thumb
    steering + Right Thumb accelerating + Second Right finger tapping Handbrake).

================================================================================
3. SCHEME (B): BLUETOOTH HID GAMEPAD MAPPING (PROBE/HIDHOST)
================================================================================
Designed for external Bluetooth gamepads (DualShock 3/4, Xbox Wireless, 8BitDo, iPega).
Connected via BCM2076B1 UART (0x3db00000) at 2.4 MBaud on L2CAP PSM 0x0013.
Input reports parsed through hid_decode() into hid_pad (probe/hidhost/hid_parse.h).

When hh_bt_state() == HH_CONNECTED:
  - 100% of on-screen touch buttons, virtual sticks, and HUD clutter are HIDDEN.
  - Full 432x240 screen dedicated to 3D scene rendering + pristine minimal game HUD.

[B.1] DIRECT 1:1 DUALSHOCK / HID GAMEPAD MAPPING TABLE:
+-------------------+--------------------+------------------------+------------------------+
| Gamepad Input     | hid_pad Bit / Axis | reVC Action (ON-FOOT)  | reVC Action (VEHICLE)  |
+-------------------+--------------------+------------------------+------------------------+
| Left Stick X/Y    | axis[0], axis[1]   | Ped Walk / Run 360°    | Steer Left / Right     |
| Right Stick X/Y   | axis[3], axis[4]   | Camera Orbit / FreeLook| Turret / Heli Pitch    |
| Cross (A)         | buttons bit 0      | Sprint (hold) / Stomp  | Accelerate (Gas)       |
| Square (X)        | buttons bit 2      | Jump / Climb           | Brake / Reverse        |
| Circle (B)        | buttons bit 1      | Attack / Fire / Punch  | Drive-by Shooting      |
| Triangle (Y)      | buttons bit 3      | Enter / Exit Vehicle   | Exit Vehicle / Bail    |
| L1 (Left Bumper)  | buttons bit 4      | Cycle Weapon Left      | Radio Station Prev     |
| R1 (Right Bumper) | buttons bit 5      | Lock-on Target / Aim   | Handbrake              |
| L2 (Left Trigger) | axis[2] or bit 6   | Sniper / Rocket ZoomOut| Look Left (Glance L)   |
| R2 (Right Trigger)| axis[5] or bit 7   | Sniper / Rocket ZoomIn | Look Right (Glance R)  |
| L2 + R2 (Both)    | bits 6 & 7 simult. | Look Behind (ped)      | Look Behind (vehicle)  |
| L3 (L-Stick Click)| buttons bit 8      | Crouch / Duck          | Horn / Siren / Taxi Sub|
| R3 (R-Stick Click)| buttons bit 9      | Look Behind            | Look Behind            |
| D-Pad Left        | hat == 6 or bit 11 | Cycle Weapon Prev      | Radio Station Prev     |
| D-Pad Right       | hat == 2 or bit 12 | Cycle Weapon Next      | Radio Station Next     |
| D-Pad Up          | hat == 0 or bit 13 | Answer Phone / Call In | Sub-mission Start (Alt)|
| D-Pad Down        | hat == 4 or bit 14 | Crouch (Alternate)     | Sub-mission Cancel     |
| Start             | buttons bit 10     | Pause Menu (Resume/Map)| Pause Menu             |
| Select / Back     | buttons bit 11/15  | Cycle Camera View      | Cycle Camera View      |
+-------------------+--------------------+------------------------+------------------------+

[B.2] LATENCY, POLLING & ENGINE FEEDER:
  - Polling Rate: BCM2076B1 UART DMA RX interrupt processes ACL packets at ~125-200 Hz.
  - Driver Buffer: Low-level reports decoded in BT Uart RxLoop (0x081920ac) into active hid_pad.
  - Input Latency: Measured transfer + decoding latency is <= 6.5 ms, well below 60 Hz frame
    interval (16.6 ms) and 30 Hz target frame interval (33.3 ms).
  - Feeder Function in Pad.cpp:
      void CPad::UpdatePads_Bluetooth(void) {
          hid_pad p;
          if (!hh_bt_pad(&p)) return;
          NewState.LeftStickX  = p.axis[0];
          NewState.LeftStickY  = -p.axis[1];
          NewState.RightStickX = p.axis[3];
          NewState.RightStickY = -p.axis[4];
          NewState.Cross       = (p.buttons & (1<<0)) ? 255 : 0;
          NewState.Circle      = (p.buttons & (1<<1)) ? 255 : 0;
          NewState.Square      = (p.buttons & (1<<2)) ? 255 : 0;
          NewState.Triangle    = (p.buttons & (1<<3)) ? 255 : 0;
          NewState.LeftShoulder1  = (p.buttons & (1<<4)) ? 255 : 0;
          NewState.RightShoulder1 = (p.buttons & (1<<5)) ? 255 : 0;
          NewState.LeftShoulder2  = (p.buttons & (1<<6)) ? 255 : 0;
          NewState.RightShoulder2 = (p.buttons & (1<<7)) ? 255 : 0;
          NewState.LeftShock   = (p.buttons & (1<<8)) ? 255 : 0;
          NewState.RightShock  = (p.buttons & (1<<9)) ? 255 : 0;
          NewState.Start       = (p.buttons & (1<<10)) ? 255 : 0;
          NewState.Select      = (p.buttons & (1<<11)) ? 255 : 0;
      }

================================================================================
4. SCHEME (C): HYBRID MAPPING (PHYSICAL + TOUCH + ACCELEROMETER TILT)
================================================================================
The optimal native iPod nano 7G control scheme. It exploits the device's physical buttons
and 100Hz LIS3DH accelerometer to eliminate screen clutter, giving tactile feedback for
rapid actions and natural tilt steering for vehicles.

Orientation & Ergonomics:
  - iPod held in Landscape mode (432 px wide x 240 px high).
  - Left edge of portrait becomes the TOP edge in landscape:
    * Volume Up, Play/Pause, and Volume Down sit right under the LEFT INDEX FINGER!
    * They function as tactile shoulder buttons (L1, L2, L3) without any screen blocking!
  - Sleep/Wake button sits on the RIGHT EDGE under the RIGHT INDEX FINGER.
  - Home chin button sits on the LEFT EDGE under the LEFT PALM/THUMB.

[C.1] PHYSICAL BUTTON ASSIGNMENT:
  1. Volume Up (GPIO 0x28)     : Next Weapon (on foot) / Radio Next (in vehicle).
  2. Volume Down (GPIO 0x29)   : Prev Weapon (on foot) / Radio Prev (in vehicle).
  3. Play/Pause (PMU reg 3 b3) : Crouch / Duck (on foot) / Horn & Siren & Taxi (in vehicle).
  4. Sleep/Wake (PMU reg 3 b1) : Pause / Game Menu (Start).
  5. Home Chin (PMU reg 3 b0)  : Cycle Camera Mode (Select).

[C.2] ACCELEROMETER TILT STEERING (LIS3DH / I2C1 @ 0x08a91e50):
  - In vehicles, tilt roll (device tilt along Y-axis in landscape) replaces the virtual steer stick.
  - Sampler: 100Hz direct read of global 0x08a91e50 (int32_t raw[3]).
  - Steering Filter Math:
      float raw_g = raw[1] * (1.0f / 65536.0f);  // Y-axis roll in landscape
      float delta_g = raw_g - g_neutral_y;       // Center calibration
      // Deadzone ±0.04G (~2.3 degrees of tilt prevents tremor drift)
      float steer = 0.0f;
      if (fabsf(delta_g) > 0.04f) {
          float sign = (delta_g > 0.0f) ? 1.0f : -1.0f;
          steer = sign * (fabsf(delta_g) - 0.04f) / (0.45f - 0.04f);
          steer = fminf(fmaxf(steer, -1.0f), 1.0f);
      }
      // Low-pass EMA filter (alpha = 0.20, ~3.5 Hz cutoff) + slew rate limiter:
      filtered_steer = 0.20f * steer + 0.80f * prev_steer;
      NewState.LeftStickX = (int16_t)(filtered_steer * 32767.0f);
  - Latency: 10ms sampling + 1.5ms transfer = ~11.5ms total latency. Ultra-responsive.
  - Tap Rejection: Sharp ±0.5G impulses from touchscreen taps are rejected by clamping
    max slew rate: |steer[t] - steer[t-1]| <= 0.08 per frame.

[C.3] STREAMLINED HYBRID TOUCH HUD (432x240):
Because physical buttons handle Weapon Cycle, Radio, Crouch, Horn, Pause, and Camera View,
and tilt handles Steering, the on-screen touch interface requires only 4 minimal buttons!

ON-FOOT HYBRID SCREEN:
+--------------------------------- 432 px ----------------------------------+
| [Phys Home]                              [HUD: Cash / Stars]      [Phys]  |
|  -> Camera                                                        [Sleep] |
|                                                                   ->Menu  |
|   +---------------+                                             [Enter]   |
|   | Floating Stick|       CENTRAL ACTION CORRIDOR               (380,65)  |
|   |  (Move/Walk)  |       (Pristine 200px Width)         [Attack]         |
|   |  r=40px zone  |                                      (330,125) [Sprint]|
|   +---------------+                                                (380,135)|
|                       Swipe Open Area -> Camera Aim         [Jump]        |
|  [Phys: Vol+/Play/Vol- on top edge -> WpnNext / Duck / WpnPrev] (330,185) |
+---------------------------------------------------------------------------+ 240

IN-VEHICLE HYBRID SCREEN (TILT ACTIVE):
+--------------------------------- 432 px ----------------------------------+
| [Phys Home]                              [HUD: Radar / Speed]     [Phys]  |
|  -> Camera                                                        [Sleep] |
|                                                                   ->Menu  |
|   [Glance L]  [Glance R]                                        [ExitCar] |
|   (20,90)     (80,90)     TILT DEVICE TO STEER                  (380,65)  |
|                                                                           |
|   [Tap Both -> Look Behind]   (Left thumb entirely free!  [Handbrake]     |
|                                No on-screen steering!)     (330,125) [Gas]|
|                                                                    (380,135)|
|                                                             [Brake]       |
|  [Phys: Vol+/Play/Vol- on top edge -> RadioNext / Horn / RadioPrev]       |
+---------------------------------------------------------------------------+ 240

Hybrid Scheme Advantages:
  - Viewport Occlusion: Dropped from 38% screen coverage (Scheme A) to < 14%.
  - No Accidental Weapon Switches: Physical volume rocker click replaces inaccurate corner swipes.
  - Immediate Horn / Ducking: Physical center Play/Pause button triggers instantly.
  - Zero Thumb Collisions: Left thumb is 100% idle while driving; right thumb only controls
    Gas/Brake/Handbrake pedals.

================================================================================
5. COMPREHENSIVE CONTROLS COMPARISON MATRIX
================================================================================
+----------------------+----------------------+---------------------+----------------------+
| Action Required      | (A) Touch-Only       | (B) BT HID Gamepad  | (C) Hybrid (Nano 7G) |
+----------------------+----------------------+---------------------+----------------------+
| Move Stick (Foot)    | Floating Stick (L)   | Left Analog Stick   | Floating Stick (L)   |
| Steer Stick (Car)    | Horizontal Zone (L)  | Left Analog Stick   | LIS3DH Accel Tilt    |
| Camera Stick         | Drag Open Screen     | Right Analog Stick  | Drag Open Screen     |
| (1) Attack / Fire    | On-screen Circle     | Circle / B (or RT)  | On-screen Circle     |
| (2) Jump / Brake     | On-screen Square     | Square / X (or LT)  | On-screen Square     |
| (3) Sprint / Gas     | On-screen Cross      | Cross / A           | On-screen Cross      |
| (4) Enter / Exit Car | On-screen Triangle   | Triangle / Y        | On-screen Triangle   |
| (5) Handbrake (Car)  | On-screen R1 Tab     | R1 / RB             | On-screen R1 Button  |
| (5b) Crouch / Duck   | On-screen Pill / 2Tap| Left Stick Click L3 | Physical Play/Pause  |
| (6) Look Back        | On-screen Top Tab    | R3 Click / L2+R2    | Glance Tabs / L2+R2  |
| (7) Radio Station    | Top Bar Swipe        | D-Pad L/R / L1      | Physical Vol+ / Vol- |
| (8) Weapon Cycle     | Weapon HUD Tap/Swipe | D-Pad L/R / L1/R1   | Physical Vol+ / Vol- |
| (9) Aim / Lock-On    | On-screen ShoulderTab| R1 / RB (or LT)     | Screen Drag / Tap Aim|
| (10) Horn / Siren    | On-screen Icon       | Left Stick Click L3 | Physical Play/Pause  |
| Pause / Menu         | Top-Left Button      | Start Button        | Physical Sleep/Wake  |
| Camera View Cycle    | Menu/Overlay Option  | Select Button       | Physical Home Button |
| Viewport Visibility  | 62% clear            | 100% clear (clean)  | 86% clear            |
| Hardware Complexity  | Zero extra hardware  | Needs BT Gamepad    | Standalone iPod Nano |
+----------------------+----------------------+---------------------+----------------------+

================================================================================
6. REVC M7/M8 IMPLEMENTATION ROADMAP & TEST HARNESS
================================================================================
1. CPad Input Feeder Multiplexer (src/core/Pad.cpp):
   - Check if hh_bt_state() == HH_CONNECTED:
       -> If TRUE: call CPad::UpdatePads_Bluetooth(), suppress all touch overlays.
       -> If FALSE: read physical GPIOs (0x3cf000a4) & PMU buttons; read LIS3DH tilt
          if in vehicle; update on-screen touch state machine from SSP2 ring buffer.
2. NanoEmu Desktop Verification Harness:
   - NanoEmu maps PC keyboard + mouse directly to the simulated hardware:
     * Arrow Keys / WASD -> Simulated LIS3DH tilt (I2C1 regfile) or LeftStickX/Y.
     * Mouse Left Drag   -> SSP2 Multi-touch packet injection.
     * Space / Shift / Ctrl / F -> Jump / Sprint / Attack / Enter.
     * Host Bluetooth Socket (/tmp/nano-bt.sock) -> Emulates BCM2076B1 HID reports.
3. Verification Proof Gate for M7/M8:
   - Walk player character in a 360-degree circle using touch/tilt/BT gamepad.
   - Enter vehicle, drive through Ocean Beach hairpin turns with smooth tilt steering.
   - Fire drive-by weapon, sound horn, cycle radio stations without frame drops or stalls.
