# M8 NanoOS port

## Plan and progress
1. Audit RetailOS dependencies and build the title/loading app: done.
2. Fix unaligned controller copies; verify Home return/re-entry: done.
3. MBX clear and interpolated single triangle in NanoEmu: passed.
4. Textured quad: passed exact red/green/blue/yellow texel orientation checks.
5. Integrate NanoOS `model.cpp` backend and UV-bearing world geometry: done.
6. Use Claude's newly supplied `mbx.h` submission/MMU/completion APIs: passed
   the complete emulator regression.

## Exact build and run
From `/home/pavliukov/ipodwork/ViceNano/m8`:
```sh
make -j4
make test-nanoemu
```
Default outputs:
- `/home/pavliukov/ipodwork/ViceNano/m8/build-nanoos/m8-nanoos.bin`
- `/home/pavliukov/ipodwork/ViceNano/m8/build-nanoos/m8-nanoos.elf`
- `/home/pavliukov/ipodwork/ViceNano/m8/build-nanoos/m8-nanoos.hbapp`
- `/home/pavliukov/ipodwork/ViceNano/m8/build-nanoos/nanoemu-stock.log`
- `/home/pavliukov/ipodwork/ViceNano/m8/build-nanoos/screens/stock/`

The test overlays Blocks in the existing snapshot, opens the chainloader,
confirms STOCK, and enters ViceNano from the NanoOS launcher. It verifies title,
clear, single triangle, textured quad, model uploads, world output, Home return
and re-entry. Original disk and NanoOS sources/build products remain unchanged.
No hardware flashing or installation is performed.

Volume+ / Right cycles title -> triangle -> textured quad -> world -> title.
Volume- / Left cycles backward. Home returns to the launcher.

Build without a GPU driver: `make GPU_BACKEND=stub -j4`. Outputs use
`build-nanoos-stub/`; rendering modes report unavailable rather than succeeding.
The rendering regression requires the default MBX backend. `make retail` retains
the previous GLES build via `Makefile.retail`.

Requires ARM bare-metal GCC/binutils, Python/Pillow and NanoOS's existing
NanoEmu/snap6/disk/resource assets. Override `NANOOS=/path/to/nanoos` and
`NANOAPPS=/path/to/NanoApps` when needed.

## Claude driver integration / shim ABI v2
`nanoos_gpu_mbx.cpp` now includes the actual sibling `src/mbx.h` and links
`src/mbx.c`. These are the driver-owned calls M8 consumes:
- `mbx_init()` establishes the GPU MMU; `MBX_VA(pointer)` maps DRAM pointers.
- `mbx_begin()` starts the scene.
- `mbx_submit(words, count)` commits each TA segment.
- `mbx_render(target, width, height, stride)` ends TA and kicks the render job.
- `mbx_wait_done(timeout_us)` performs a bounded job wait.

Claude supplied these APIs during the texture implementation. M8 no longer owns
its own page table, render-job kick or completion loop. It saves/restores all
eight MMU roots, MMU control and target registers around app ownership. Because
current `mbx_render()` kicks even if its TA wait fails, M8 always drains the job
with `mbx_wait_done()` before reporting that failure. This is a driver handoff
note: preferably stop the kick on TA failure or expose an explicit abort/reset.
No direct Claude messaging tool is available; this file records the interface
and integration findings in the shared workspace.

M8's C-compatible interface is declared in `nanoos_shim.h`:
- `m8_gpu_init/begin/triangle/end/shutdown`: original screen-space lifecycle.
- `m8_gpu_texture_create(w,h,format,pixels)`: copies linear 16-bit pixels into
  owned twiddled storage; positive handle or negative failure. Format 1 RGB565,
  format 2 ARGB4444. Power-of-two sizes 8..32; 32 live handles maximum.
- `m8_gpu_texture_destroy(handle)`: releases a handle between scenes.
- `m8_gpu_textured_triangle(handle, vertices)`: copies normalized UVs and
  screen-space positions/colors into TA packets. No caller pointer retained.
- `m8_gpu_status`: frame/triangle/error/timeout counters for QMP checks.

The local adapter owns texture packing and draw packets: vertex format `0x2503`
(xyz, packed AABBGGRR, uv), ISP textured bit 17, Morton order with y in the low
bit, RGB565/ARGB4444 format state and MODULATE combiner `0x86084610`.
Clear is an oversize GPU triangle. Nearest/repeat sampling is used. CPU data
cache remains disabled; ordered writes precede submission.

Still needed from Claude: acquire/release of an idle native LCD back buffer
(pointer, stride, orientation, present), so the completed private GPU target can
be presented without a pixel copy. Native layout is
`native[landscape_x*240+239-landscape_y]`. Driver reset/abort and mapping teardown
would replace M8's local register restoration. Hardware TA parameter buffers,
FIFO space checks and cache maintenance remain driver work; this is emulator
validation only.

## model.cpp and embedded world
`model.cpp` selects `nanoos_model.inc` under `M8_NANOOS`; RetailOS/RenderWare code
remains in its existing branch. The NanoOS backend implements `loadBlock`,
`unloadBlock`, `m8PrepareBatches`, `renderWorld` and the existing statistics over
`NanPack`/`NanPart`/`NanTexture`. It validates indices and bounds, uploads raw
RGB565/RGBA4444 or existing PAL8 textures (up to 32x32), converts RGBA4444 to
ARGB4444, clips the near plane while interpolating UVs/colors, projects vertices,
sorts triangles and submits textured FIFO draws. Unload releases texture handles
and subtracts resident byte counts. Current frame budget is 128 source triangles,
with room for 256 triangles after clipping.

`nanoos_world.cpp` constructs a real NanPack/NanPart view of the bounded local
asset sample and invokes those model APIs. `tools/make_world_preview.py` keeps
112 material-distributed triangles from `nbdecoshop01`, UVs quantized to 1/256,
positions to 1/16 world unit, and all 26 real textures downsampled to 8x8 RGB565.
World vertices use neutral white modulation. This keeps the sample in the
existing emulator slot while exercising multiple textures and indexed geometry.
Generated assets remain under build-nanoos and retain their original user-owned
asset restrictions; they are not portable redistributable game assets.

This is a textured world sample, not full engine parity. Remaining work:
perspective-correct interpolation/VGP, GPU depth (currently painter ordering),
alpha/translucency, full-resolution textures, multiple streamed blocks, camera
controls and audio. Pass-through UVs are affine, and painter ordering can show
intersections incorrectly. ARGB4444 upload is implemented but the visual
regression currently exercises opaque RGB565 only.

## Payload packaging
The snapshot Blocks slot is 20,708 bytes. Textures exceeded the previous raw
wrapper budget, so `nanoos_chainloader.c` decodes an embedded LZSS kernel into
a bounded 1 MiB staging buffer, then calls the unchanged sibling shutdown and
SRAM handoff. It does not search filesystem payload paths. Kernel address/layout
remains `0x08000000`, under NanoOS's 64 KiB code limit and 2 MiB kernel/BSS/stack
reservation.

`tools/pack_kernel.py` verifies a byte-for-byte independent decoder round trip
before writing the compressed payload. Tokens use a 4 KiB history, literal flag
bits and bounded match lengths up to 273 bytes. The C decoder checks input,
output/history bounds and final alignment padding. Successful NanoEmu boots also
exercise the C decoder. The `.bin` output remains the uncompressed kernel.

The integrated launcher keeps DevMode stats and omits its unrelated Drivers
probe page. `alignas(4) CPad pad` remains necessary because NanoOS retains CPU
alignment checking and the compiler copies controller state with word accesses.

## Dependency audit
- `viewer.cpp`: `gl_app_init/frame` are invoked by the SDK GL surface lifecycle;
  `hb_gl_surface` creates the RetailOS EAGL context and swaps buffers. The overlay
  uses GLES textures, vertex arrays, blending and an SDK font atlas.
- `nano.cpp`: RenderWare GLES engine/camera lifecycle, fixed-function GL state and
  triangle draws; `hb_fpu_attach_current_task` opts a RetailOS task into VFP;
  `hb_time_uptime_us` supplies frame timing and profiling.
- `model.cpp`: RenderWare raster allocation and GLES texture upload (PAL8 with
  RGB565/RGBA4444 fallback), matrices, client arrays and indexed/array rendering.
- `stream.cpp`, `model.h`, `input.cpp`: portable bounded residency, asset contracts,
  and controller/touch logic. No RetailOS calls.
- `input.h`: portable HID report contract. `viewer.cpp` gets multitouch through
  `hb_touch_*` and Bluetooth through `hh_bt` (RetailOS transport/tasks).
- `audio.cpp`: portable mixer over embedded assets; output adapter allocates from
  `hb_os_alloc`, borrows DMA channel 2 and RetailOS IRQ handling, calls SVC 70 to
  clean caches and restores OS registers on exit. This cannot run under NanoOS.
- `memory.h` and `../sdk/runtime.cpp`: tracked malloc/free, global new/delete and
  constructor iteration ultimately depend on the RetailOS heap.
- Assets are compiled into `generated/*.h`; runtime streaming does not open files.
  `fprintf(stderr)` is the viewer's only runtime stdio. Converter file I/O is a
  host build concern, not a payload dependency.
- `Makefile`: SDK relocatable hbapp startup, GL surface and runtime resolution.

## Validation results
- Default and unavailable-driver stub builds pass; no undefined kernel symbols.
- Compression round trips pass. The compressed wrapper boots in NanoEmu.
- Before the driver API migration, the textured quad's four exact texel checks,
  textured model screenshot, model counters (26 uploads, 3,328 resident bytes),
  zero GPU errors/timeouts and Home return/re-entry passed.
- PASS final regression against Claude's driver API: 395 completed GPU frames,
  14,244 submitted triangles including clears, zero errors/timeouts, job status
  `0x44`. Model counters: 26 texture uploads, 3,328 resident bytes, 112 draw calls
  and 336 vertices/indices in the captured world frame. Exact quad colors and
  world texel diversity checks pass; Home return/re-entry pass.
- Final raw kernel is 26,904 bytes. Compressed `.hbapp` is 20,612 bytes, fitting
  the 20,708-byte emulator slot with 96 bytes remaining. Larger assets still
  require a larger emulator slot or external assets; compression is not a
  replacement for a full streaming filesystem.
- `git diff --check` passes and the kernel has no undefined symbols.
- Captures: `m8_mbx_triangle_landscape.png`, `m8_mbx_quad_landscape.png`,
  `m8_mbx_world_landscape.png` under `build-nanoos/screens/stock/`.
