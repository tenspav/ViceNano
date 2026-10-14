# ViceNano M0–M1 triangle bring-up

Destination blocked: `/home/pavliukov/ipodwork/ViceNano/m0/` is outside the
session's writable roots; `mkdir` returned `Read-only file system`. Deliverables
are staged here in `/tmp/`, as allowed by the request. Nothing was installed
in ViceNano. Copy this directory to `ViceNano/m0/` in a writable session.

## Build and execution

`make` uses the requested `nano7-untethered/nanoapps/sdk/hb_app.mk`,
`GL_SURFACE=1`, Cortex-A5 Thumb, soft-float, and the SDK's Thumb-FP gate.
GCC: arm-none-eabi-gcc 10.3.1 (20210621). Build passed without warnings;
Thumb-FP gate passed. `tri.hbapp`: 2,848 bytes, HRL1, six relocations;
image 2,800 bytes; loader span 68,340 bytes (includes SDK linker spacing).
`build.log` records the complete command and packaging output.

`tri.c` follows probe/gltest's resident-owned GL context, perspective matrix,
vertex/color arrays and frame-based rotation (2 degrees/frame). No GL call
runs in `gl_app_init`. One RGB triangle uses `glDrawArrays`, depth test,
no texture/lighting/blending/culling. It uses the ARM SDK and resident RetailOS,
then NanoEmu's MBX model; it does not substitute desktop GL.

Run `./run-in-emulator.sh`. Requires the existing NanoEmu build, snap6 dumps,
resource image, ARM compiler, Python 3 and Pillow. The helper copies the disk,
uses the existing disk_patch.py's FAT lookup and HRL1-aware padding, applies
the patch records only to the copy, and marks the copy read-only. NanoEmu is
launched directly to place state/screens below this directory (the standard
run.sh defaults would place them in NanoRework). QMP over standard input/output supplies touch input and screenshots without
network sockets. A timed 150 ms Home press at guest second 8 exits the capture app.
The helper reads the selected page dot and swipes in the appropriate direction
to page 2, then taps Blocks using the Session 31 launch coordinates.
The screenshot check requires the expected blue background, colored geometry,
and changes between frames. Screenshots remain available for visual review.
An existing run/disk-copy.dd is refused unless `--reuse-copy` is supplied;
reuse verifies the replaced blocks against a freshly generated expected patch
for the current app. Archive/move run/ when changing the input disk. The snapshot inputs are read-only; source-disk size/mtime and
SHA-256 of replaced source blocks are checked after execution. This is not a
full-disk hash. No physical device, SDK, emulator source, or dump is modified.

## Issues encountered

Destination creation failed due to the session filesystem policy. VNC socket
creation also failed with `PermissionError: [Errno 1] Operation not permitted`;
the final helper uses socket-free QMP pipes. Initial launch attempts omitted the reference command's continued MMU/table
and torn-snapshot exclusion settings. They left DumpRAM unchanged; the checker
correctly failed (zero expected-background pixels, zero changed pixels). The
final launcher includes `mmu=0x08b28000` and
`torn-ignore-pc=0x093d2a80:0x093d4100`, and uses a full 150 ms Home press. Page detection also corrects
the initial assumption that Home returned to page 1: this snapshot returns
to page 3. The failed run is retained in failed-launch-evidence/.

SDK repository HEAD: `7a0f097650f50cf2fbd244984e340db8c04827e3`.
NanoRework repository HEAD at inspection: `b1c41d80a0f11b4cfbac5b14c55a9698f0301377`.
The runtime uses the available working-tree binary; HEAD alone does not prove
its build provenance. `sdk-sha256.json` records the SDK source hashes used.

## Verified result

PASS: the requested colored triangle renders and spins in NanoEmu. Visual
inspection of run/triangle-0.png confirms one RGB triangle on dark blue.
Three 240x432 screenshots contain 90,808–90,809 expected-background pixels
and 11,621–11,626 colored pixels. Compared with frame 0, subsequent captures
change 16,746 and 17,444 pixels. run/check.json records PASS and the app hash.
NanoEmu completed 258 scenes, 258 renders and 258 render jobs during the short
run, without a guest panic/assert. Its summary still appends `(nothing drawn)`;
that diagnostic suffix is stale: the screenshots visibly contain the triangle.
This short smoke run establishes neither long-term stability nor device behavior.

Source disk size/mtime and hashes of all six replaced source blocks were
unchanged. The five recorded SDK source hashes also remained unchanged.
Only the disk copy was patched; guest NAND writes remained in the emulator's
in-memory page store. Logs, command line, patch and screenshots are under run/.

## SDK findings

* hb_app.mk hardcodes Cortex-A8; a local `override HB_ARCH_FLAGS` before its
  include selects Cortex-A5 consistently for compile, link and libgcc lookup.
* glGetString returns synthetic identity/capability strings, so cannot prove
  GLES features or extensions.
* glAlphaFunc/glNormal3f write through gl_current_gc; the reference investigation
  identifies this as the renderer rather than the driver's actual context.
  Neither is used here. Fixes are deferred; SDK edits were outside scope.
* Native glDrawElements defaults on; its disabled branch draws nothing.
  This triangle uses glDrawArrays and does not validate indexed rendering.

## Execution-plan gate verdicts

| Gate item | Verdict |
|---|---|
| M0 pinned librw 0ad67ee… populated | NOT RUN; outside this minimal triangle deliverable |
| M0 unchanged miami reVC build | NOT RUN |
| M0 IMG/DIR inventory tool | NOT RUN |
| M0 PC 1.0 assets, baseline rendering, decoded sizes | NOT RUN; no asset processing performed |
| M1 GL_SURFACE=1 SDK .hbapp | PASS: packaged ARM app, Thumb-FP gate passed |
| M1 visible triangle rotation in NanoEmu (requested smoke test) | PASS: three screenshots and 258 completed render jobs |
| M1 rotating textured cube | NOT MET: requested untextured triangle only |
| M1 VBO | NOT TESTED: client arrays |
| M1 ushort indices / repeated native indexed draws without hangs | NOT TESTED: glDrawArrays |
| M1 depth and visible occlusion | PARTIAL: depth enabled; single triangle cannot prove occlusion |
| M1 culling | NOT TESTED: disabled |
| M1 blending | NOT TESTED: disabled |
| M1 explicit alpha test | NOT TESTED; known SDK context issue remains |
| M1 stable heap | NOT MEASURED; no per-frame app allocation, driver heap unmeasured |
| M1 physical-device validation | NOT RUN; emulator-only scope |

Overall M0 and full M1 are NOT COMPLETE. This deliverable establishes only the
requested toolchain/triangle bring-up; subsequent cube and capability probes
remain required. Destination placement is blocked by the workspace policy.
