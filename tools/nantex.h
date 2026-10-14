/*
 * nantex.h - NanTex Texture Archive Format for ViceNano (PowerVR MBX Lite GLES1)
 *
 * Designed for iPod nano 7G (Apple M2 SoC, PowerVR MBX Lite GLES 1.1).
 * Supports 16bpp (RGB565, RGBA4444, RGBA5551) and 8bpp paletted (GL_OES_compressed_paletted_texture).
 */

#ifndef NANTEX_H
#define NANTEX_H

#include <stdint.h>

#define NANTEX_MAGIC       0x544E414E /* "NANT" in little-endian (0x4E, 0x41, 0x4E, 0x54) */
#define NANTEX_VERSION     1
#define NANTEX_MAX_MIPS    12
#define NANTEX_NAME_LEN    32

typedef enum {
    NANTEX_FMT_RGB565        = 1, /* 16bpp: R5 G6 B5 (GL_RGB, GL_UNSIGNED_SHORT_5_6_5) */
    NANTEX_FMT_RGBA4444      = 2, /* 16bpp: R4 G4 B4 A4 (GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4) */
    NANTEX_FMT_RGBA5551      = 3, /* 16bpp: R5 G5 B5 A1 (GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1) */
    NANTEX_FMT_PAL8_RGBA8888 = 4, /* 8bpp: 1 B/px index + 256*4B RGBA palette (GL_PALETTE8_RGBA8_OES) */
    NANTEX_FMT_PAL8_RGBA4444 = 5, /* 8bpp: 1 B/px index + 256*2B RGBA4444 palette (GL_PALETTE8_RGBA4_OES) */
    NANTEX_FMT_PVRTC4        = 6  /* 4bpp: PowerVR compressed texture (if supported) */
} NanTexFormat;

#pragma pack(push, 1)

/* File Header: 16 bytes */
typedef struct {
    char     magic[4];        /* "NANT" */
    uint16_t version;         /* Format version (1) */
    uint16_t flags;           /* Flags (reserved, 0) */
    uint32_t texture_count;   /* Number of textures in archive */
    uint32_t reserved;        /* Alignment padding / reserved (0) */
} NanTexHeader;

/* Per-Texture Entry: 156 bytes */
typedef struct {
    char     name[NANTEX_NAME_LEN]; /* Texture name (ASCII, null-terminated/padded) */
    uint16_t width;                 /* Base width at mip level 0 */
    uint16_t height;                /* Base height at mip level 0 */
    uint16_t format;                /* NanTexFormat enum value */
    uint16_t mip_count;             /* Number of stored mip levels (1..NANTEX_MAX_MIPS) */
    uint32_t filter_flags;          /* Original RenderWare filter/addressing flags */
    uint32_t data_offset;           /* Absolute byte offset in file where texture data begins */
    uint32_t data_size;             /* Total byte size of all mip levels + palette */
    uint32_t palette_offset;        /* Offset relative to data_offset (0 if no palette) */
    uint32_t palette_size;          /* Size in bytes of palette (e.g. 1024 or 0) */
    uint32_t mip_offsets[NANTEX_MAX_MIPS]; /* Offset of each mip level relative to data_offset */
    uint32_t mip_sizes[NANTEX_MAX_MIPS];   /* Byte size of each mip level */
} NanTexEntry;

#pragma pack(pop)

#endif /* NANTEX_H */
