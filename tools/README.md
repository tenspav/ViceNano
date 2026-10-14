# ViceNano Texture Tooling: txdconv

Texture conversion tool for **ViceNano** (GTA Vice City on iPod nano 7G with PowerVR MBX Lite GLES 1.1).

Converts PC RenderWare `.txd` texture archives into custom `.nantex` containers tailored for MBX Lite VRAM and filtering constraints.

## Features

- **Input Formats**: Direct3D 8/9 RenderWare TXD dictionaries (DXT1, DXT3, DXT5, 32bpp BGRA, 24bpp BGR, 16bpp C1555/C565/C4444, and 8bpp PAL8).
- **Target Formats (MBX Native)**:
  - **RGB565** (`NANTEX_FMT_RGB565`, 16bpp): Used for opaque textures.
  - **RGBA4444** (`NANTEX_FMT_RGBA4444`, 16bpp): Used for alpha/translucent textures.
  - **PAL8** (`NANTEX_FMT_PAL8_RGBA8888`, 8bpp + 256 RGBA palette): Used for decals, UI, and particles with $\le 256$ colors or quantized to cut memory by 50% using `GL_OES_compressed_paletted_texture`.
- **Mipmap Generation**: Produces complete mip chains down to 1x1 for MBX hardware min-filter modes (`GL_LINEAR_MIPMAP_NEAREST`, `GL_LINEAR_MIPMAP_LINEAR`).
- **Validation**: Strict integrity verification checking all headers, offsets, payloads, and per-mip dimensions.
- **C Header**: Includes `nantex.h` for zero-copy/direct struct mapping in the M2-M3 C renderer.

## Usage

```bash
# Convert a TXD archive to .nantex
./txdconv input.txd output.nantex

# Inspect TXD contents without converting
./txdconv --info input.txd

# Validate an existing .nantex file
./txdconv --validate output.nantex

# Optional flags:
# Force all alpha textures to RGBA4444:
./txdconv input.txd output.nantex --alpha-format 4444

# Force all alpha textures to PAL8 (quantized to 256 colors):
./txdconv input.txd output.nantex --alpha-format pal8

# Downscale textures larger than 256x256 (recommended for screen budget):
./txdconv input.txd output.nantex --max-dim 256

# Keep only base mip level (no downsampling):
./txdconv input.txd output.nantex --mips none
```

## .nantex Specification

Refer to `nantex.h` for struct definitions:
- **Header** (16 bytes): Magic `NANT`, version `1`, flags, texture count.
- **Entry Table** (156 bytes per texture): Name (32 bytes), base dimensions, format ID, mip count, filter flags, data offset, payload size, palette offset/size, and 12 mip offset/size slots.
- **Payload**: 4-byte aligned texture payload containing palette (if paletted) followed by packed mip level data.
