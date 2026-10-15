# M5 — bounded world streaming

**PASS for the selected three-cell world-streaming test in NanoEmu.** This report follows the requested M5 world-streaming scope; the older `docs/exec-plan.md` assigns streaming to M6–M7. No physical-device RAM claim is made. In particular, this reduced set fitting below 18 MiB does not establish that RetailOS can supply a full 18 MiB allocation window.

The viewer retains M4's reduced reVC fakerw/clump/texture helpers and the librw GLES1 client-array pipeline. The [source check](source-check.json) verifies 23 fakerw and four texture/clump helper functions against the read-only reference. No VBO creation/deletion path is used. `stream.cpp` replaces the single-model lifecycle with camera-driven cell requests, two resident slots, cache hits that refresh age, and eviction of the least recently used slot. Eviction destroys geometry/clumps before texture dictionaries. A load failure cleans up the partially built slot; capacity admission happens before eviction.

The camera moves continuously around three real North Beach IPL placements. A 140-unit forward lookahead requests the corresponding 100-unit cell. Empty cells preserve the bounded warm cache. All resident blocks render in their original relative world positions; this is not three separately centered/spinning model previews. Every 108 frames, a full unload audits application allocation balance and guest free heap, without resetting cumulative load/eviction counters or restarting the engine.

## Local asset selection

[Provenance](asset-provenance.json) records the IDE/IPL source hashes, joined placement records, IMG sectors, extracted sizes and SHA256 for `nbdecoshop01` (4187), `nbdecoshop02` (4186), and `nbdecoshop03` (4188). The cells are `(4,-9)`, `(4,-10)`, and `(3,-10)` after converting Z-up to Y-up. The packer bakes DFF frame transforms, IPL scale/quaternion/translation, and coordinate conversion, preserving world scale. It extracts DFF/TXD entries from the user's local IMG with `rb` only. Textures are reduced to POT RGB565/RGBA4444, capped at 64×64.

Together the three packs contain 58 material parts, 49 textures, 8,517 packed vertices, and 5,214 triangles: 204,408 vertex bytes, 31,284 index bytes, and 401,408 texture bytes (637,100 total). These are a sparse building subset of three adjacent cells, not complete city blocks with all street furniture/terrain/LODs.

The packs are embedded read-only backing storage. Each slot loads/evicts decoded geometry, client-array instance data, CPU raster shadows, and GL textures through the reduced pipeline. NAND read-at/sector streaming and the original `CStreaming` implementation are not part of this milestone's implementation.

## Budget and checks

The tracked RW allocator enforces an 18 MiB live-allocation cap, including old/new overlap during realloc and lazy geometry instancing. Admission separately reserves eight times packed vertex bytes, four times index bytes, three times texture bytes, 4 KiB per material part, and 64 KiB per block. Adding a 2 MiB executable/engine/stack/display allowance gives a maximum two-block policy envelope of **4,581,160 bytes (4.37 MiB)**. This is conservative admission policy for this asset set, not a measurement of every hidden GPU allocation. OS free heap is sampled after frame work; instantaneous internal driver upload peaks are not directly sampled. RW instrumentation does include transient application allocations.

[Host checks](host-check.log) pass an explicit LRU-hit/victim test, over-budget rejection before allocation, empty-cell handling, 30 traversals with 300 traversal loads (plus three initial LRU-test loads), repeated full unloads, and idempotent exit. RW peak is 771,374 bytes, final live allocation is zero, and all 5,239 texture uploads have matching deletes. The host GL mock validates lifetime/calls; NanoEmu validates rendering and guest heap behavior.

[Package checks](package-check.log) verify HRL1 bounds, 1,273 relocations and nine packed relative constructors; the ARM ELF has no undefined symbols. The Cortex-A5 Thumb soft-float gate passes. The final package is 721,212 bytes, with a 716,096-byte image, 762,232-byte span and 46,136 BSS bytes. The original executable slot reserves 1,111,628 bytes regardless of package size.

## NanoEmu evidence

[Guest telemetry verification](run/capabilities.json) passes all 16 screenshots, covering frames 346–710. Every captured frame advances; all GL-error, balance and supported-state badges are green, with zero allocation failures. The last capture records **66 block loads, 64 total evictions, and 6 full-unload audits**. Total evictions include audit clears; approximately 52 are capacity-driven LRU evictions. There are more than 20 additional load/evict transitions within the captured interval itself.

Two distinct streamed views: [shop 03 / cell (3,-10)](run/vcstream-0.png) and [shop 02 / cell (4,-10)](run/vcstream-1.png). The first view has 11,131 visible model pixels and 346 colors above the telemetry. Resident masks include all three two-block combinations, plus a one-block state immediately after an audit. These are the same ARM package, SDK bindings, RetailOS and emulated MBX used by M4; no host renderer substitutes for the guest.

| Measurement | Guest result |
|---|---:|
| RW unloaded baseline | 2,464 B |
| RW allocation peak, including transient application work | 745,998 B |
| Warm full-unload OS free heap | 6,480,784–6,480,904 B |
| Full-unload heap range | 120 B |
| Largest warmed heap range for an identical resident set | 7,516 B |
| Lowest sampled OS free heap | 5,670,604 B |
| Conservative observed main-heap residency cost (baseline high minus trough) | 810,300 B |

RW live bytes return exactly to baseline at every full audit and match their previous value for each warmed resident mask. OS heap varies slightly with driver/system activity and the periodic keep-awake input; verification allows a bounded 8 KiB resident-set range and 512 B unloaded range, and the measured results above are smaller. This is bounded memory evidence across repeated transitions, not a claim of byte-exact driver accounting or zero instantaneous upload overhead. The main-heap footprint and conservative admission policy both remain well below 18 MiB. Physical-device availability of an 18 MiB pool and hidden shared-GPU accounting remain unproved.

[Disk integrity and scene-change checks](run/check.json) pass: original disk size, mtime, and replaced-block hashes are unchanged. The final package SHA256 is `08981f722125507ef35058e3b570008dd1789a884f70da3cd9a5f2d0233e4447`.

Two diagnostic attempts are retained in [diagnostics](diagnostics/). An initial overlay issued hundreds of small draws per frame; the next attempted batch exceeded the existing 1,024-vertex Im2D limit. The final overlay uses batches of 128 quads (512 vertices). Long unattended runs also reached the guest's halt loop; the final QMP runner sends periodic harmless canvas touches to prevent RetailOS idle sleep. These changes are confined to the viewer and host runner; no SDK or emulator patch is used.

The original disk opens read-only and receives an in-memory executable overlay. The runner checks original disk size, mtime and hashes of every replaced block. No physical device, SDK, or reference reVC tree is modified. No commit is made.

## Reproduction

From `m5/`: `python3 prepare-assets.py`, `./check-host.sh`, `python3 check-selection.py`, `./run-in-emulator.sh`, then `python3 verify.py`. See [README](README.md) for prerequisites and counter schema. Generated user-owned assets, converters, host executables and ARM packages are ignored.
