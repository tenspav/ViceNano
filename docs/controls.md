# ViceNano (reVC on iPod nano 7G) Input Architecture & Controls Mapping
1. reVC INPUT ABSTRACTION (src/core/Pad.h, Pad.cpp, ControllerConfig.h):
   - CPad updates each frame in UpdatePads(); holds CControllerState (DualShock layout: Left/Right sticks, DPad, Cross/Square/Circle/Triangle, L1/R1/L2/R2/L3/R3, Start/Select).
   - Game queries high-level getters (GetPedWalkUpDown/LeftRight, GetAccelerate, GetBrake, GetWeapon, etc.). In PC builds, ControlsManager reconciles keyboard/mouse into CPad.
   - Menus (Frontend.cpp) poll CPad buttons (Cross/Enter to select, Triangle/Circle to go back, DPad to navigate) or direct mouse coordinates (NewMouseControllerState).
2. FULL VICE CITY ACTION SET:
   - On-foot: Move (analog 2D), Sprint, Jump, Attack/Fire, Lock-on/Aim, Duck, Enter/Exit car, Weapon Next/Prev, Look Behind, Action/Phone.
   - Driving: Steer Left/Right, Accelerate, Brake/Reverse, Handbrake, Exit car, Drive-by Fire, Horn/Siren, Submissions (Taxi/Vigilante), Look L/R/Behind, Radio Prev/Next, Turret/Heli pitch.
   - System/Camera: Cycle Camera View, Free-look/Aim (analog 2D), Pause/Menu navigation (Up/Down/Left/Right/Select/Back).
3. CONCRETE CONTROL SCHEME (Landscape 432x240):
   - Driving: Tilt steer via 100Hz LIS3DH accelerometer (EMA smoothed, ±0.04G deadzone -> LeftStickX); touch right pedals (Gas=Cross, Brake=Square, Handbrake=R1, Exit=Triangle); touch left for Look L/R.
   - On-Foot: Left-screen floating virtual stick -> LeftStickX/Y; right-screen touch buttons (Sprint=Cross, Jump=Square, Fire=Circle, Enter=Triangle); swipe top corner for Weapon Cycle.
   - Physical buttons: Vol+/Vol- = Weapon Cycle / Camera; Play-Pause = Horn / Crouch; Home/Power = Pause / Menu (Start).
   - BT HID Gamepad (docs/research/bluetooth-stack.md): Direct 1:1 DualShock CControllerState mapping (auto-hides on-screen touch UI when connected).
   - Menus & NanoEmu: Direct single-touch tap/drag -> CMouseControllerState (LMB + X/Y); NanoEmu emulator maps mouse to touch and PC keyboard to DualShock keys.
4. M8 MILESTONE FIRST IMPLEMENTATION:
   - Implement minimal CPad::Update() feeder: left-screen virtual stick (walk/steer) + 3 touch buttons (Gas/Sprint, Brake/Jump, Enter/Exit) + Power/Home for Pause (Start).
   - Hook 100Hz accel tilt polling (0x08a91e50) to vehicle LeftStickX and NanoEmu WASD/Arrow keys; verify walk/drive route before building full touch HUD or BT HID stack.
