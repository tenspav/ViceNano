M8 renderer results, 2026-10-15.

PAL8 uploads, runtime fallback, and cached indexed material batches are implemented. Both full NanoEmu scenarios pass every non-FPS gate. **M8 remains FAIL: neither scenario sustains 24+ FPS.**

All times below are microseconds per frame, averaged using the existing screenshot telemetry verifier over 24 samples. These are fresh before/after runs; the supplied 17.45 FPS context, committed 20.55 FPS capture, and pre-existing modified DUMPRAM captures are different baselines. The fresh baseline uses the committed atomic renderer with the inherited O3 build configuration.

| Scenario | Version | guest_fps_average | draw_calls_us | stream_upload_us | audio_mixing_us | input_processing_us |
|---|---|---:|---:|---:|---:|---:|
| Stock (342/133 MHz) | Before | 18.73 | 21659 | 9 | 345 | 98 |
| Stock | PAL8 + batches | 19.97 | 17334 | 8 | 399 | 61 |
| Unlocked (684/266 MHz) | Before | 17.33 | 24227 | 6 | 3855 | 2770 |
| Unlocked | PAL8 + batches | 20.56 | 16867 | 12 | 733 | 373 |

Stock FPS increased 6.6%, with 20.0% less draw time. Unlocked FPS increased 18.6%, with 30.4% less draw time. These are single runs of the existing scripted input scenarios, not device measurements or statistically established speedups. Input is applied per frame, so the final camera positions need not be identical between builds.

| Gate | Stock | Unlocked |
|---|---|---|
| sustained_fps_24plus | FAIL | FAIL |
| All other 12 checks | PASS | PASS |

The passing checks include 500+ observed touches (558 stock / 534 unlocked), player movement, camera movement, no touch heap growth, audio, no guest errors, the RW budget, supported states, balanced accounting, source-disk integrity, changing images, and advancing frames. Full results are in [stock-pass/capabilities.json](stock-pass/capabilities.json) and [pass/capabilities.json](pass/capabilities.json); before runs are in [before-stock](before-stock/) and [before-unlocked](before-unlocked/).

PAL8 is the implemented compressed-upload option. `prepare-assets.py` uses the shared `txdconv.encode_pal8_16` encoder and NanTex writer, generating palette/index headers and `.nantex` files. Opaque textures use `GL_PALETTE8_R5_G6_B5_OES` (0x8b97); alpha textures use `GL_PALETTE8_RGBA4_OES` (0x8b98). The standalone converter also accepts `--texture-format pal8`. Existing 32x32 sampling and geometry resolution are retained.

| Texture storage | Before, bytes | After, bytes |
|---|---:|---:|
| All 49 converted textures: packed asset payload | 100,352 | 75,264 |
| Initial resident block: packed asset payload | 53,248 | 39,936 |
| Initial block: librw CPU texture shadow | 53,248 | 0 |
| Initial block: expanded driver texels, estimated | 53,248 | 53,248 |

Packed texture data falls 25%. **PAL8 does not deliver PVRTC VRAM savings:** this RetailOS driver expands palette indices to 16-bit texels. The driver-texel row and telemetry `texture_resident_bytes` are decoded-size accounting, not a measured GPU allocation query; driver copies/overhead are additional. CPU shadow removal is real: the compression-only run reduces initial tracked RW heap from 310,344 to 257,096 bytes. With batching it falls to 87,952 bytes. Initial unlocked OS free heap rises from 6,007,504 to 6,236,472 bytes. The measured module span falls from 895,968 to 883,840 bytes. All three models are converted; their texture payloads are 39,936 / 19,968 / 15,360 bytes. Separate start-position smoke builds exercised the remaining 13 and 10 textures in NanoEmu, with zero fallbacks/errors and valid rendered frames; [other-blocks-validation.json](other-blocks-validation.json) records those checks. Together with the main route, all 49 world textures were accepted by the compressed upload path.

An upload error deletes any partial texture, expands the stored palette/indices into temporary RGB565/RGBA4444 pixels, retries `glTexImage2D`, and frees the scratch buffer. A failed retry unwinds the load. `M8_CPPFLAGS=-DM8_FORCE_TEXTURE_FALLBACK` exercises the fallback. NanoEmu accepted 26 initial-block uploads with zero fallbacks normally; the forced run took 26 fallbacks with zero errors.

[texture-validation.json](texture-validation.json) compares against [the committed M8 reference](baseline-reference/initial.png). In the unobscured world region `(0,264)-(240,300)`, PAL8 has mean absolute channel error 2.19/255; batching and forced fallback each change zero pixels relative to the PAL8 image. Uploads explicitly use M8's requested NEAREST sampling; differences from the legacy reference can include palette quantization and sampler state. This is a bounded screenshot comparison, not an all-view image-quality proof. The current shared-workspace build also passes [current-build-check/check.json](current-build-check/check.json).

The old custom atomic pipeline expanded every triangle index into an `Im3DVertex` array and revisited atomic/material state while drawing. The converter already reduced one model from 22 parts to 13 material groups; simply regrouping that model again would save no additional calls. The new runtime stores compact 24-byte vertices and ushort indices, shares identical texture payloads, and prepares material groups and index streams for all two-slot visibility masks when residency changes. Transparent draws remain ordering barriers. Groups split at the ushort limit. Per-material bounds are computed once, then tested against the camera each frame.

The initial model has 26 materials, 2,655 source vertices, and 4,896 indices. The block-only indexed version still submits 26 draws. Material bounds reduce the initial visible submission to 18 draws, 2,350 vertices, and 4,272 indices, preserving the comparison image exactly. Final sampled draw counts range from 18 to 21. Most materials in this route use distinct textures, limiting further same-material merging. The host test explicitly proves two identical resident packs share uploads and combine into one draw per material.

The remaining FPS gap is unclosed. At 20.56 FPS the smoothed frame period is about 48.6 ms, while the measured draw section averages 16.9 ms. Overlay work, presentation and RetailOS/emulator scheduling are not separately attributed here. These results do not establish 24 FPS on hardware. No reVC core, librw, SDK GL wrapper, or NanoRework source changes were needed.

`m8/check-host.sh` passes 100 residency/render/unload cycles, texture/material sharing, all index-mask bounds, translucent ordering, ushort splitting, off-screen material rejection, compressed rejection, double upload failure, and balanced CPU/texture lifetimes. PAL8 exact-palette roundtrips, quantization payload sizes, all three generated NanTex containers, and standalone CLI mip-chain conversion also passed.

Reproduction uses the existing NanoRework `run.sh` with all writable state, screenshots, audio, and NAND overlays directed outside NanoRework. Both final scenarios use emulator SHA-256 `d783b9530334268b73f2ede7bd3c58d19a155bf23e93f7e030ec83614d3e59a4`. Source-disk size, mtime, and replaced-block hashes remain unchanged.

```sh
m8/check-host.sh
m8/run-in-emulator.sh stock
m8/run-in-emulator.sh unlocked
```

The FPS measurements use [vcperf-measured.hbapp](vcperf-measured.hbapp), SHA-256 `5bb72b868a9feeee78f86764570cce76e7828ee75dca3001361b0a051123e3cd`. [measured-source.tar.gz](measured-source.tar.gz) preserves the isolated source used for those runs, including the original audio implementation held constant before/after. Another editor changed `audio.cpp` during this session; that independent edit is preserved in the current workspace, whose rebuilt `vcperf.hbapp` has a separate successful smoke capture. The current workspace therefore has no claim to the measured binary's exact FPS.

Commits could not be created: the session mounts `.git` read-only, and `git add` failed creating `.git/index.lock` with “Read-only file system.” Ordered review patches are in [changes](changes/); the final source is already applied. No push was attempted. Initial and overlapping-edit snapshots are preserved under `/tmp/vicenano-m8-start` and `/tmp/vicenano-m8-concurrent-snapshot`.
