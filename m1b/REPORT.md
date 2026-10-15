# M1b — textured cube and native indexed drawing

PASS in NanoEmu: the same ARM HRL1 app, resident RetailOS SDK bindings and MBX emulation render a rotating RGB565 checker cube using two VBOs and client ushort indices. No desktop GL replacement is used. Outward winding, backface culling and depth occlusion are visible. Eight screenshots demonstrate rotation and changing indexed-draw counters.

Proof: [cube-0](run/cube-0.png), [cube-7](run/cube-7.png), [pixel checks](run/capabilities.json), [launch and source preservation](run/check.json). Run `./run-in-emulator.sh --reuse-copy` then `python3 verify.py`. The disk copy is patched and read-only during execution; the source disk's size, mtime and replaced-block hashes are unchanged.

Alpha rejection/acceptance and source-alpha blending pass pixel checks. The emulated blend result is RGB (112,0,112); the test explicitly records this quantized result rather than claiming ideal 8-bit blending. A local SDK gl.c copy fixes glAlphaFunc/glNormal3f to use OGL TLS's actual driver context instead of the renderer object. Alpha is verified; normal/lighting are not verified here. Original SDK sources remain unchanged.

After startup warm-up, OS free heap is exactly 7,277,564 bytes in all eight captures across 14 seconds. Visible telemetry records baseline and current free bytes, no GL errors and an 8-bit native draw counter. This establishes short-run emulator stability, not available device RAM or long-duration stability. The additional telemetry itself also uses indexed draws. No app allocation occurs per frame.

The ARM Cortex-A5 Thumb soft-float build passes the SDK Thumb-FP gate. A compact local linker script removes the GNU default 64 KiB text/data gap, allowing the app to fit the Blocks slot. See build.log for size and relocation counts. GL resource creation happens only in gl_app_frame.

M1b's requested textured-cube and repeated-indexed-draw gates are closed. Physical-device validation and the original M0 PC baseline/asset gates remain unverified; no physical device was touched. Continuing to M2.
