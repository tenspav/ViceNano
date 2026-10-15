# M3 — first converted Vice City model through the reVC renderer adapter

PASS in NanoEmu: a spinning textured Vice City traffic cone renders through the default librw GLES1 atomic pipeline. The viewer invokes real reVC fakerw camera/state/atomic functions, copied verbatim into an explicit five-function adapter subset. This is a standalone model viewer, not full reVC game initialization.

Proof: [model frame](run/vcmodel-0.png), [later frame](run/vcmodel-7.png), [pixel/reload/allocation evidence](run/capabilities.json), [rotation and original disk preservation](run/check.json). Eight frames pass texture, rotation, native indexing, no-error, balanced-unload and changing reload-count checks. NanoEmu completed 467 render jobs. The stale emulator `(nothing drawn)` suffix is contradicted by framebuffer proof.

## Assets and conversion

Read-only user assets: `NanoRework/games/vc-files/models/gta3.img` and `gta3.dir`. `trafficcone.dff` (sector 31449; 2024 chunk bytes) and `dyncones.txd` (sector 30678; 5832 chunk bytes) are extracted by DIR sector metadata, then decoded with pinned librw on the host. DFF SHA256: `07dbfda36c9f5d6120ca3d78d6a34ac5fe282f6c78e475c9799df8039851d32a`. TXD SHA256: `b2cec0b6cd53a7ff7980b0b9c23f3350d246492f15296a0397fabbb079267721`.

The converted asset has one part, 25 vertices, 20 triangles, 600 packed vertex bytes, 120 ushort index bytes and one POT `redwhite_stripe` 64x64 RGB565 texture (8192 bytes). The converter supports RGBA4444 for textures with alpha, bounds texture dimensions to 128, bakes frame transforms, changes Z-up to Y-up and centers/scales the model for viewing. Source prelight colors remain in the pack; the unlit viewer uses white vertex RGB so night prelighting does not obscure the texture. No game asset is tracked for redistribution: generated DFF/TXD/header and hbapp outputs are ignored.

## Lifecycle and memory

Host test: 100 load/render/unload cycles return RW allocations to baseline; textures and CPU vertex caches are released. Host baseline: 6140 bytes; peak: 18252 bytes. A separate RGBA4444 upload also passes. The host GL recorder verifies API/lifecycle behavior; guest screenshots verify actual rendering.

ARM screenshot telemetry: loaded RW allocation balance is 13,144 bytes; unloaded baseline is 2,464 bytes. Every reload checks Geometry/Atomic/Texture counts and RW balance before loading again; failures latch a visible badge. Loaded OS free heap is exactly **6,477,068 bytes in all eight frames**, across reload counts 2 to 3. Unloaded heap samples are 6,488,648 and 6,488,640 bytes (8-byte variation), giving an observed main-heap asset cost of 11,572–11,580 bytes. The texture's 8192-byte packed source remains in the executable, plus an 8192-byte CPU raster shadow and driver upload storage. The OS heap query does not fully account for GPU internal pools; full shared-memory/upload-duplication accounting remains an M5 task.

The default atomic pipeline caches interleaved CPU vertices per material and uses native ushort indexed draws. A VBO experiment lost about 1000 heap bytes per reload despite deletion; detaching arrays did not resolve it. Explicit zero-sized storage release reduced the loss but raised GL errors. That path is not enabled. See [experimental evidence](vbo-retirement.json). M1b's VBO drawing gate remains proven; VBO retirement needs SDK/driver investigation before streamed VBO geometry is enabled. The deployed client-array pipeline shows no loaded-heap growth in this run.

## Build and reproduction

Run `./prepare-assets.sh`, `./check-host.sh`, `./run-in-emulator.sh`, then `python3 verify.py` from this directory. The preparation helper accepts `--assets`, `--model` and `--txd` for other local entries. No host libraries or desktop GL are used in the ARM image.

ARM Cortex-A5 Thumb soft-float package: 86,024 bytes, image 82,008, span 111,584, 998 relocations. Thumb-FP gate passes; `package-check.log` verifies all nine relative constructors are present in the HRL1 image. The resident invokes all GL resource work inside its current-context frame callback. Native/foreign geometry is rejected by the default static pipeline; skinning, dynamic lighting, full material effects and mipmaps remain future work.

The runner uses the original read-only disk with an in-memory NAND overlay for the existing Mario Kart app slot, retaining filesystem metadata. The padded executable load occupies 1,111,628 bytes; this slot padding is included in the running footprint. Source disk size, mtime and every patched block's source hash remain unchanged. Neither the physical device, SDK nor reference reVC tree was modified.

M1b–M3's named emulator renderer gates are closed. M4's full reduced reVC source selection, texture/clump helpers and Nano init/frame/exit adapter remain the next milestone; only its small fakerw renderer subset is present here. Device validation and sustained performance are not claimed.
