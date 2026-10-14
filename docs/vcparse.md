e reader → `Raster::convertTexToCurrentPlatform`.
- Exact [`src/raster.cpp:475`](https://raw.githubusercontent.com/mrxenginner/librw/0ad67ee226ea51beb3f465cb8857195e4db16029/src/raster.cpp): DXT fast path `d3d_to_gl3` preserves/flips compressed blocks only when DXT is supported.
- Otherwise `toImage()` → unconditional `unpalettize()` → `imageFindRasterFormat` → `create` → `setFromImage`, repeated for mips; D3D [`rasterToImage`](https://raw.githubusercontent.com/aap/librw/master/src/d3d/d3d.cpp) swizzles BGRA and decodes DXT1/3/5 through `Image::setPixelsDXT`.
- Backend must implement `rasterCreate/Lock/Unlock/NumLevels`, `imageFindRasterFormat`, `rasterFromImage`, `rasterToImage`; install Driver callbacks and native raster lifecycle. Exact [`gl/gl3raster.cpp`](https://raw.githubusercontent.com/mrxenginner/librw/0ad67ee226ea51beb3f465cb8857195e4db16029/src/gl/gl3raster.cpp) is the template.
- There, `rasterFromImage` converts/flips rows; `rasterUnlock` calls `glTexImage2D` or `glCompressedTexImage2D`. Existing GLES handling forces RGBA8888; replace it and add 565/4444 packing, correct stride/mip sizes.
- 16bpp uploads: `GL_RGB/GL_UNSIGNED_SHORT_5_6_5`, `GL_RGBA/GL_UNSIGNED_SHORT_4_4_4_4`; GLES1 internalformat equals format. Preserve alpha and texture orientation.
- PAL8 requires bypassing unconditional unpalettization and GL3 palette rejection; upload palette + all indices/mips together with `glCompressedTexImage2D`, `GL_PALETTE8_*_OES`, level `1−mipCount`. [OES specification](https://registry.khronos.org/OpenGL/extensions/OES/OES_compressed_paletted_texture.txt).
- PVRTC requires a native format discriminator, reader/writer/size handling and compressed upload; extension-gate PVRTC1 2/4bpp and implement minimum mip storage sizes. [IMG specification](https://registry.khronos.org/OpenGL/extensions/IMG/IMG_texture_compression_pvrtc.txt).
- Extend `Texture::streamReadNative/streamWriteNative/streamGetSizeNative` for the backend; merely changing TXD format bits cannot represent PAL8/PVRTC upload semantics.

3. **Geometry and MBX consumption**
- Generic DFF stores separate float3 position/normal arrays per morph target, RGBA8 prelighting, float2 UV arrays (up to eight), and uint16 triangle indices/material IDs; no single on-disk vertex stride. [`geometry.cpp`](https://raw.githubusercontent.com/aap/librw/master/src/geometry.cpp).
- [`gl/gl3pipe.cpp::defaultInstanceCB`](https://raw.githubusercontent.com/aap/librw/master/src/gl/gl3pipe.cpp) interleaves position, optional normal/color, UVs: stride `12 + 12N + 4C + 8T`; typical one-UV meshes use 24 or 36 bytes with color.
- These types map directly to GLES1 vertex/normal/color/texcoord arrays; separate arrays can avoid interleaving. Build per-material uint16 draw indices; DFF triangle records cannot be submitted directly. [GLES1.1 specification](https://registry.khronos.org/OpenGL/specs/es/1.1/es_full_spec_1.1.pdf).
- GLES1 has fixed attribute roles, not generic shader attributes; query `GL_MAX_TEXTURE_UNITS`, map available UV sets, use CPU skinning absent a verified palette extension. MBX-specific stride/alignment/performance limits require device tests; no justified universal 32-byte limit.

4. **Offline tools versus device work**
- Build `vicenano-txd`: decode PC native/DXT, resize to supported POT dimensions, generate alpha-aware mips, select RGB565/RGBA4444/PAL8, quantize a shared palette across mips, optionally encode PVRTC1; emit backend-native TXDs.
- Build `vicenano-dff-opt`: uninstance foreign native data, remove unused vertices/materials/attributes, optimize material batches/index order, optionally simplify LODs and prepack aligned arrays; preserve required Skin/HAnim/MatFX/frame metadata.
- Add an IMG/DIR repacker when converted asset sizes change. On-device retain parsing, reference resolution, bounded upload/repacking, CPU skinning and small 16-bit fallback conversions; perform palette quantization, mip generation, PVRTC encoding and mesh simplification offline.
