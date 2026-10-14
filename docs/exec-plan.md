t` and `MakeSpaceFor/RemoveModel`.
SDK `hb_fs_read` only reads from the beginning: implement verified persistent seek/read-at support before IMG streaming; start synchronously, then add bounded per-frame I/O. Show loading progress.
Keep RetailOS resident for SDK GL. If it cannot provide the envelope, reduced residency or an OS-memory project is required; bare-metal chainloading cannot retain these OS GL entry points.

| Work | Estimate, one developer | Main risk |
|---|---|---|
| M0–M1 | 2–5 days | Dependencies, SDK ABI and indexed/state verification |
| M2–M3 | 2–4 weeks | RW lifecycle/state semantics, raster conversion and atomic rendering |
| M4 | 1–3 weeks | Freestanding C++, relocation/runtime coverage and dependency removal |
| M5 | 3–7 days measurement; potentially weeks of remediation | RetailOS ownership, fragmentation and hidden driver allocations |
| M6–M7 | 3–6 weeks | Seek API, asset reduction, expanded-memory accounting and streaming stalls |
| M8–M9 | 6–12+ weeks | CPU skinning/simulation cost, controls, mission assumptions and audio; full-game performance remains unproven |
tokens used
61,628
**Execution order: SDK cube → librw asset viewer → reduced reVC → bounded streamed world → gameplay.** First 3D is M1; first VC model is M3.
The embedded [analysis](/home/pavliukov/ipodwork/NanoRework/docs/research/revc-gles1-port.md) informs this plan, but two assumptions need correction: librw is an empty `vendor/librw` submodule, and available game RAM is unproven.

| Milestone | Goal / what compiles | Acceptance proof | Runs |
|---|---|---|---|
| M0 | Populate pinned librw `0ad67ee…`; build unchanged `miami` reVC and an IMG/DIR inventory tool. | PC 1.0 assets load; record baseline rendering and decoded asset sizes. | PC host |
| M1 | Build `GL_SURFACE=1` SDK `.hbapp`: rotating textured cube, VBO, ushort indices, depth/culling/blending. | Visible rotation and occlusion; repeat native indexed draws without hangs; stable heap. | NanoEmu → device |
| M2 | Build standalone librw GLES1 library + cube harness: Device/Driver, camera, states, Im2D/Im3D. | Same cube through RW; correct winding/projection; overlay and scene coexist. | PC harness; NanoEmu → device |
| M3 | Build default atomic/raster pipelines and host converter; extract one static DFF/TXD from the user's IMG. | Converted POT RGB565/RGBA4444 textured VC model spins; load/unload returns allocations to baseline. | Converter: PC; viewer: NanoEmu → device |
| M4 | Build reduced reVC `.hbapp`, explicit source list, Nano platform adapter and C/C++ runtime shims. | `fakerw` + reVC texture/clump helpers render M3's asset; init/frame/exit work without desktop dependencies. | PC reference; NanoEmu → device |
| M5 | Build allocator/GL allocation instrumentation and memory stress harness. | Establish usable RAM with RetailOS alive; account for code/BSS, stacks, GPU and loading peaks; prove a ≤40 MiB operating envelope or revise target. | NanoEmu → device |
| M6 | Build reduced-asset packer, `FileMgr` adapter and `CdStream_nano.cpp`; integrate bounded streaming. | Correct random sector reads; request/load/evict cycles; worst converted entry fits scratch; no growth during repeated traversal. | PC tools/reference; NanoEmu → device |
| M7 | Build static world viewer: IDE/IPL/COL, sector visibility, LODs, streaming, deterministic moving camera. | Representative dense-area route renders with texture+world residency and transition peaks inside M5's envelope. | PC capped allocator; NanoEmu → device |
| M8 | Build CPU skinning, player/collision/physics, camera and touch/button input; limited vehicles/peds. | Walk/drive a repeatable route with stable memory and measured device frame times. | PC; NanoEmu → device |
| M9 | Build script VM/mission startup, save/load and bounded audio; restore necessary effects incrementally. | Representative missions complete; sustained traversal, save/reload and audio remain within budget. | PC regression; NanoEmu → device |

Fork librw `src/gl/gl3device.cpp`, `gl3immed.cpp`, `gl3pipe.cpp`, `gl3render.cpp`, `gl3raster.cpp` and `rwgl3.h`/`rwgl3impl.h` into a separate `src/gles1/` backend; add platform selection in engine/base headers and CMake. [Pinned device source](https://raw.githubusercontent.com/mrxenginner/librw/0ad67ee226ea51beb3f465cb8857195e4db16029/src/gl/gl3device.cpp).
Replace shader uniforms/attributes with fixed matrices, materials, vertex/color/normal/UV arrays and VBOs; omit `gl3shader.cpp`. Later adapt `gl3skin.cpp` for reusable CPU skinning and `gl3matfx.cpp` for simplified environment mapping.
Rewrite raster upload for 16-bit textures and offline mipmaps; remove GL3 FBO/VAO requirements and its GLES RGBA8888 promotion. [Pinned raster source](https://raw.githubusercontent.com/mrxenginner/librw/0ad67ee226ea51beb3f465cb8857195e4db16029/src/gl/gl3raster.cpp).
Device bridge: include SDK `gl.h` with C linkage and link `gl.c`; defer every GL allocation/call until `gl_app_frame`, where the resident owns the current context and presentation.
NanoEmu bridge: run the **same ARM `.hbapp` + SDK bindings + RetailOS**, exercising emulated MBX commands; no replacement host-GL shortcut. A separate PC compatibility harness can accelerate renderer debugging.
M1 must validate actual capabilities: `glGetString` is synthetic; native indexing is currently enabled, while disabling it draws nothing. Fix direct-context writes in `glAlphaFunc`/`glNormal3f`; test alpha explicitly; defer fog and unsupported effects.
For fastest boot, M1–M3 bypass `CGame::Initialise` entirely. M4 uses a dedicated viewer entry with zeroed input, deterministic timer/camera and one asset.
Add a link-complete NULL audio backend; exclude OpenAL/Miles/mpg123, movies/cutscenes, frontend/HUD, mission initialization/processing, population/traffic, custom shader pipelines, postfx and droplets. Preserve interfaces needed by retained code.
Extend SDK build rules for C++ and required libc/math/allocation support; override the current Cortex-A8 flags for Cortex-A5 while preserving the SDK's soft/softfp calling convention.
Budget proposal, **including shared GPU RAM**: textures 12 + geometry 6 + code/game/pools/collision/scripts 10 + audio 3 + streaming/decode 3 + display/depth 1 + reserve 5 = **40 MiB**.
M3 measures upload duplication; M5 proves availability; M6 proves loading peaks; M7 and M8 prove the resident textured world fits. Budget the active streamed window, not the whole city.
Replace both 65 MiB streaming assignments in `CStreaming::Init2`; clamp vehicle residency independently, bound largest-entry buffers, and make admission/eviction use actual decoded CPU+GPU allocations.
NAND hooks: implement `CdStreamRead/GetStatus/Sync/AddImage` behind the existing 2048-byte sector API; retain `RequestModelStream → ProcessLoadingChannel → ConvertBufferToObject` and `MakeSpaceFor/RemoveModel`.
SDK `hb_fs_read` only reads from the beginning: implement verified persistent seek/read-at support before IMG streaming; start synchronously, then add bounded per-frame I/O. Show loading progress.
Keep RetailOS resident for SDK GL. If it cannot provide the envelope, reduced residency or an OS-memory project is required; bare-metal chainloading cannot retain these OS GL entry points.

| Work | Estimate, one developer | Main risk |
|---|---|---|
| M0–M1 | 2–5 days | Dependencies, SDK ABI and indexed/state verification |
| M2–M3 | 2–4 weeks | RW lifecycle/state semantics, raster conversion and atomic rendering |
| M4 | 1–3 weeks | Freestanding C++, relocation/runtime coverage and dependency removal |
| M5 | 3–7 days measurement; potentially weeks of remediation | RetailOS ownership, fragmentation and hidden driver allocations |
| M6–M7 | 3–6 weeks | Seek API, asset reduction, expanded-memory accounting and streaming stalls |
| M8–M9 | 6–12+ weeks | CPU skinning/simulation cost, controls, mission assumptions and audio; full-game performance remains unproven |
