# M4 — reduced reVC Nano viewer

PASS for reduced reVC rendering, host/ARM lifecycle and memory checks. The requested localhost:5905 listener remains blocked by the sandbox socket policy; framebuffer proof uses the same ARM NanoEmu guest over QMP.

The explicit [source selection](source-selection.json) compiles 23 verbatim reVC fakerw functions and four verbatim `RwHelper.cpp` texture/clump helpers, plus the Nano platform adapter, packed-model loader, SDK bridge and freestanding runtime. [Source verification](source-check.json) records the reference hashes and verifies each selected function body. Includes replace reVC's broad `common.h`; the reference tree remains read-only. This is a reduced viewer entry, bypassing `CGame::Initialise` as required by the milestone plan.

`NanoInit`, `NanoFrame`, `NanoUnload` and `NanoExit` own the engine/camera/model lifecycle. The model lives in an `RpClump` and a texture dictionary; material binding uses `RwTexDictionaryFindNamedTexture`, helper callbacks retrieve the first atomic/texture, and rendering enters `RpClumpRender`. Exit destroys the clump before its dictionary, restores the previous current dictionary, destroys the camera/frame/raster and stops/closes/terminates RW. Repeated unload/exit calls are supported. No input is read; frame numbers drive a deterministic camera-relative rotation. Every 90 guest frames the viewer executes complete exit/init and latches any allocation imbalance on screen. All engine and GL resource work occurs inside the resident's current-context frame callback. The SDK provides no exit callback; periodic frame-context shutdown tests the exit adapter without changing the resident or SDK.

The retained runtime supplies constructors, C++ new/delete, malloc/calloc/realloc/free backed by the OS heap, math and freestanding libc. File syscalls fail explicitly because assets are embedded. The final ARM ELF has no undefined symbols. Desktop windows/GL, OpenAL/Miles/mpg123, movies, HUD/frontend, missions, traffic/population, streaming, skinning and postfx are absent from this selection. No audio interface is imported, so a NULL audio object is not needed to link this viewer.

## Second Vice City asset

`nbdecoshop01.dff` and `nbdecoshop01.txd` form a North Beach shop building (IDE object 4187 in `data/maps/nbeach/nbeach.ide`). Read-only extraction uses the user's IMG/DIR sector metadata. [Provenance](asset-provenance.json) records sectors, sizes and SHA256. This is a second model, separate from M3's traffic cone.

The M4 converter bakes frame transforms, changes Z-up to Y-up, centers/scales the building, compacts each material part to its referenced vertices, and caps POT textures at 64×64 RGB565/RGBA4444. The shop yields 26 material parts, 26 RGB565 textures, 2,655 packed vertices (63,720 bytes), 1,632 triangles (9,792 index bytes), and 212,992 texture bytes: 286,504 embedded asset bytes. Source prelighting remains in the pack; the unlit preview uses white RGB as in M3. No game initialization, IDE/IPL scene loading or world streaming is claimed.

## Host and packaging checks

[Host checks](host-check.log) pass 20 complete engine init/exit cycles with five model load/render/unload cycles each, including duplicate-init rejection, invalid dimensions, frame-before-init rejection and idempotent exit/unload. The shop records 2,600 uploads, 2,600 deletes and 2,600 indexed draws; peak RW allocation is 430,768 bytes and final live bytes are zero. The same reduced adapter also passes M3's cone pack: 100 uploads/deletes/draws, peak 18,388 bytes, final live bytes zero. Host GL recording validates lifecycle and calls; guest framebuffer proof validates rendering.

The Cortex-A5 Thumb soft-float `.hbapp` is 367,760 bytes, with a 363,128-byte image, 392,936-byte span and 1,152 relocations. The SDK Thumb-FP gate passes. [Package checks](package-check.log) validate HRL1 bounds and all nine packed relative constructors. Client-array indexed geometry follows M3's balanced path; the experimental VBO retirement path stays disabled.

## NanoEmu proof and limits

The requested VNC endpoint `127.0.0.1:5905` was attempted. [Failure evidence](vnc-socket-denied.log) records QEMU's `Failed to create a socket: Operation not permitted`. This sandbox prohibits sockets and provides no escalation permission. The same ARM hbapp, RetailOS, SDK bindings and emulated MBX run through QMP pipes with headless framebuffer capture. The VNC-listener requirement remains environment-blocked; no physical-device validation is claimed.

[Eight-frame verification](run/capabilities.json) passes: [shop frame](run/vcnano-0.png), [later rotation](run/vcnano-7.png), and [disk/rotation checks](run/check.json). All frames show thousands of building pixels with hundreds of texture colors, green GL-error/state/lifecycle badges, and restart counts advancing from 3 to 4. NanoEmu completed 375 render jobs; its stale `(nothing drawn)` suffix is contradicted by framebuffer proof.

RW unloaded baseline is 2,464 bytes; loaded balance is exactly 411,980 bytes in all eight frames. The complete exit path also checks zero RW live bytes before reinitializing. Unloaded OS heap is exactly 6,483,656 bytes in all samples. Loaded free heap ranges from 6,052,432 to 6,052,556 bytes (124-byte variation), corresponding to an observed main-heap model cost of 431,100–431,224 bytes. The 212,992-byte embedded texture pack remains in code storage, alongside CPU raster shadows and driver storage. This is bounded lifecycle evidence, not a complete GPU/shared-memory accounting or sustained traversal test; M5 remains responsible for that envelope.

The runner overlays the existing Mario Kart executable slot in memory. It opens the original disk read-only and verifies size, mtime and original replaced-block hashes after the run. The slot's padded executable allocation remains 1,111,628 bytes, independent of the smaller package size. The SDK, physical device and reference reVC tree were not modified. No commits were made by this worker.

## Reproduction

From `m4/`, run `./prepare-assets.sh`, `./check-host.sh`, `python3 check-selection.py`, `./run-in-emulator.sh`, then `python3 verify.py`. The default runner requests VNC on localhost:5905. In a socket-restricted environment use `./run-in-emulator.sh --headless`. Package validation: `python3 ../tools/check_hrl1.py vcnano.hbapp build/vcnano.reloc.elf`.

Generated source assets, packed headers, host executables and ARM packages are local outputs and ignored. Previously tracked `m4/generated/*` and `m4/converter` were removed from the working tree to prevent redistribution; the orchestrator should retain these deletions. `.git` is read-only to this worker, so index removal cannot be performed here. Extraction/conversion regenerates these ignored outputs from user-owned assets.
