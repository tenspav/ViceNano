# Reduced viewer controls

Build with `make -C m7`; run `m7/check-host.sh`, then `m7/run-in-emulator.sh`.
Requires M5's locally generated world assets, the existing ARM toolchain, SDK,
and NanoRework snap6/NAND resources. All output is in this repository. The runner
opens the NAND image read-only and overlays the app in memory.

The 432x240 research layout is transposed into the native 240x432 surface:
landscape (x,y) -> native (y,x). RetailOS touch queue coordinates are already
screen pixels. The adapter prefers the resident-latched SDK mailbox when available
and retains per-finger queue samples as a fallback. If a queue disappears without
an observed up, the old-snapshot fallback clears that contact after one second.
A stationary hold with no queued reports may therefore expire on older residents;
the mailbox path retains exact holds/releases. Its observed phases are 0 down, 2 move, 1 up, despite the older SDK
header's comment. Do not apply the raw digitizer formula again.

Move by dragging in the left stick zone; swipe the center to orbit/pitch the
camera. The yellow world-space diamond represents the player. RUN increases
movement speed. Other buttons populate the pressure fields for future gameplay.
This build has no ped, collision, vehicles, combat or pause menu. PAUSE/Start
also requests a connection to the default known NanoPad peer for verification.
The host then uses RetailOS pairing/encryption and the retrieved SDP descriptor.
Connected HID takes priority, clears stale touch states and hides the controls.
A disconnect returns to touch. NanoPad uses Z/Rz for its right stick; the known
peer normalization maps these into the adapter's Rx/Ry fields without treating
them as triggers. Other controllers retain the generic research mapping.

The runner injects mouse input via QMP, warms the OS touch queues with 200 moves,
then sends at least 540 moves and continues until 520 new guest-observed events.
It swipes the camera, presses Start, and captures the fake
controller's seven scripted HID reports. `/tmp/nano-bt.sock` is optional and was
not present here: the existing `bt-peer`/`bt-pad` model runs the same HCI, SDP,
pairing and encrypted L2CAP path without an external socket. No emulator patch
or direct guest pad-state writes are used.

`verify.py` decodes 16 little-endian bit rows at native y=148+4*row, x=8+7*bit.
Fields: observed touch events, HID reports, player X*16, player Z*16, camera yaw*100,
pitch*100, tracked RW live bytes, OS free bytes, RW peak, GL/frame errors,
allocation failures, HID state, Cross pressed, left X, left Y, frame.
Signed coordinate rows use two's complement. The RGB indicators at y=130 show
frame errors, balanced budget and unsupported render states.

`hh_bt.{c,h}` and `hid_parse.{c,h}` are local copies of NanoRework's Session 51
host (unchanged); they use snapshot-specific RetailOS entry points and keep
bounded descriptor/report storage. The host allocates persistent callback
trampolines once per app launch. Its app-switch reclamation and general physical
controller compatibility remain outside this reduced-viewer gate.
