#!/usr/bin/env python3
"""
txdconv - RenderWare TXD to ViceNano .nantex Texture Converter
Target: iPod nano 7G (Apple M2 SoC, PowerVR MBX Lite GLES 1.1)

Converts PC RenderWare TXD textures to MBX-native formats:
  - Opaque:       RGB565 (16bpp, GL_UNSIGNED_SHORT_5_6_5)
  - Alpha:        RGBA4444 (16bpp, GL_UNSIGNED_SHORT_4_4_4_4) or PAL8 (8bpp + 256 RGBA palette)
  - Generates mip levels for MBX min-filter modes (GL_LINEAR_MIPMAP_NEAREST, etc.)
  - Packages textures into custom .nantex container with validation
"""

import sys
import os
import struct
import argparse
import numpy as np
from PIL import Image

RESAMPLE_BILINEAR = getattr(getattr(Image, 'Resampling', None), 'BILINEAR', Image.BILINEAR)

# ---------------------------------------------------------------------------
# Constants & Enums
# ---------------------------------------------------------------------------

RW_CHUNK_STRUCT         = 0x01
RW_CHUNK_EXTENSION      = 0x03
RW_CHUNK_TEXTURENATIVE  = 0x15
RW_CHUNK_TEXDICTIONARY  = 0x16

PLATFORM_D3D8           = 8
PLATFORM_D3D9           = 9
PLATFORM_PS2            = 0x00325350 # 'PS2\0'

RW_RASTER_C1555         = 0x0100
RW_RASTER_C565          = 0x0200
RW_RASTER_C4444         = 0x0300
RW_RASTER_C8888         = 0x0500
RW_RASTER_C888          = 0x0600
RW_RASTER_PAL8          = 0x2000
RW_RASTER_PAL4          = 0x4000
RW_RASTER_MIPMAP        = 0x8000

# NanTex Format Enums (matching nantex.h)
NANTEX_MAGIC            = b"NANT"
NANTEX_VERSION          = 1
NANTEX_MAX_MIPS         = 12

NANTEX_FMT_RGB565        = 1
NANTEX_FMT_RGBA4444      = 2
NANTEX_FMT_RGBA5551      = 3
NANTEX_FMT_PAL8_RGBA8888 = 4
NANTEX_FMT_PAL8_RGBA4444 = 5
NANTEX_FMT_PVRTC4        = 6
NANTEX_FMT_PAL8_RGB565    = 7

FMT_NAMES = {
    NANTEX_FMT_RGB565:        "RGB565 (16bpp)",
    NANTEX_FMT_RGBA4444:      "RGBA4444 (16bpp)",
    NANTEX_FMT_RGBA5551:      "RGBA5551 (16bpp)",
    NANTEX_FMT_PAL8_RGBA8888: "PAL8-RGBA8 (8bpp+pal)",
    NANTEX_FMT_PAL8_RGBA4444: "PAL8-RGBA4 (8bpp+pal)",
    NANTEX_FMT_PVRTC4:        "PVRTC4 (4bpp)",
    NANTEX_FMT_PAL8_RGB565:   "PAL8-RGB565 (8bpp+pal)"
}

# ---------------------------------------------------------------------------
# RenderWare TXD Parser
# ---------------------------------------------------------------------------

class RWTexture:
    def __init__(self):
        self.name = ""
        self.mask = ""
        self.platform = 0
        self.filter_flags = 0
        self.format = 0
        self.has_alpha = 0
        self.width = 0
        self.height = 0
        self.depth = 0
        self.num_levels = 0
        self.raster_type = 0
        self.compression = 0 # 0=raw, 1=DXT1, 2=DXT2, 3=DXT3, 4=DXT4, 5=DXT5
        self.palette = None
        self.mip_data = [] # list of bytes per mip level


def parse_chunk_header(f):
    data = f.read(12)
    if len(data) < 12:
        return None
    return struct.unpack("<III", data)


def parse_txd(filepath):
    """Parses a RenderWare TXD file and returns a list of RWTexture objects."""
    with open(filepath, "rb") as f:
        file_size = os.path.getsize(filepath)
        header = parse_chunk_header(f)
        if not header or header[0] != RW_CHUNK_TEXDICTIONARY:
            raise ValueError(f"Invalid TXD file: expected chunk 0x16, got {header}")
        
        dict_size = header[1]
        
        # Read Struct chunk inside TexDictionary
        s_hdr = parse_chunk_header(f)
        if not s_hdr or s_hdr[0] != RW_CHUNK_STRUCT:
            raise ValueError(f"Invalid TXD: expected struct chunk 0x01, got {s_hdr}")
        
        num_tex, device = struct.unpack("<HH", f.read(4))
        textures = []
        
        for i in range(num_tex):
            n_hdr = parse_chunk_header(f)
            if not n_hdr:
                break
            if n_hdr[0] != RW_CHUNK_TEXTURENATIVE:
                # Might have hit extension chunk or padding
                f.seek(n_hdr[1], os.SEEK_CUR)
                continue
            
            tex_end = f.tell() + n_hdr[1]
            
            # Struct chunk inside TextureNative
            ts_hdr = parse_chunk_header(f)
            if not ts_hdr or ts_hdr[0] != RW_CHUNK_STRUCT:
                f.seek(tex_end)
                continue
            
            tex = RWTexture()
            tex.platform = struct.unpack("<I", f.read(4))[0]
            tex.filter_flags = struct.unpack("<I", f.read(4))[0]
            
            name_raw = f.read(32)
            mask_raw = f.read(32)
            tex.name = name_raw.split(b"\x00")[0].decode("latin1", errors="replace").strip()
            tex.mask = mask_raw.split(b"\x00")[0].decode("latin1", errors="replace").strip()
            
            if tex.platform == PLATFORM_D3D8:
                tex.format, tex.has_alpha, tex.width, tex.height, tex.depth, tex.num_levels, tex.raster_type, tex.compression = struct.unpack("<IIHHBBBB", f.read(16))
                
                # Check for palette
                if tex.format & (RW_RASTER_PAL4 | RW_RASTER_PAL8):
                    palsz = 32 if (tex.format & RW_RASTER_PAL4) else 256
                    tex.palette = f.read(palsz * 4)
                
                # Read mip levels
                for m in range(tex.num_levels):
                    msz = struct.unpack("<I", f.read(4))[0]
                    tex.mip_data.append(f.read(msz))
                    
            elif tex.platform == PLATFORM_D3D9:
                tex.format, d3dfmt, tex.width, tex.height, tex.depth, tex.num_levels, tex.raster_type, flags = struct.unpack("<IIHHBBBB", f.read(16))
                tex.has_alpha = flags & 1
                tex.compression = 0
                if d3dfmt == 0x31545844: # 'DXT1'
                    tex.compression = 1
                elif d3dfmt == 0x32545844:
                    tex.compression = 2
                elif d3dfmt == 0x33545844: # 'DXT3'
                    tex.compression = 3
                elif d3dfmt == 0x35545844: # 'DXT5'
                    tex.compression = 5
                
                if tex.format & (RW_RASTER_PAL4 | RW_RASTER_PAL8):
                    palsz = 32 if (tex.format & RW_RASTER_PAL4) else 256
                    tex.palette = f.read(palsz * 4)
                
                for m in range(tex.num_levels):
                    msz = struct.unpack("<I", f.read(4))[0]
                    tex.mip_data.append(f.read(msz))
            else:
                # Unsupported platform (e.g. PS2)
                f.seek(tex_end)
                continue
            
            textures.append(tex)
            f.seek(tex_end)
            
        return textures


# ---------------------------------------------------------------------------
# Texture Decompressors (DXT1, DXT3, DXT5, Uncompressed, Paletted) -> RGBA
# ---------------------------------------------------------------------------

def decode_dxt1(data, w, h):
    """Decompresses DXT1 block data into RGBA8888 numpy array (H, W, 4)."""
    bw = (w + 3) // 4
    bh = (h + 3) // 4
    raw = np.frombuffer(data, dtype=np.uint8).reshape(bh, bw, 8)
    
    c0 = raw[..., 0].astype(np.uint16) | (raw[..., 1].astype(np.uint16) << 8)
    c1 = raw[..., 2].astype(np.uint16) | (raw[..., 3].astype(np.uint16) << 8)
    
    r0 = (((c0 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
    g0 = (((c0 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
    b0 = ((c0 & 0x1F) * 255 // 31).astype(np.uint8)
    
    r1 = (((c1 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
    g1 = (((c1 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
    b1 = ((c1 & 0x1F) * 255 // 31).astype(np.uint8)
    
    colors = np.zeros((bh, bw, 4, 4), dtype=np.uint8)
    colors[..., 0, 0] = r0; colors[..., 0, 1] = g0; colors[..., 0, 2] = b0; colors[..., 0, 3] = 255
    colors[..., 1, 0] = r1; colors[..., 1, 1] = g1; colors[..., 1, 2] = b1; colors[..., 1, 3] = 255
    
    mask = c0 > c1
    c2_r = np.where(mask, (2*r0.astype(np.uint16) + r1)//3, (r0.astype(np.uint16) + r1)//2).astype(np.uint8)
    c2_g = np.where(mask, (2*g0.astype(np.uint16) + g1)//3, (g0.astype(np.uint16) + g1)//2).astype(np.uint8)
    c2_b = np.where(mask, (2*b0.astype(np.uint16) + b1)//3, (b0.astype(np.uint16) + b1)//2).astype(np.uint8)
    colors[..., 2, 0] = c2_r; colors[..., 2, 1] = c2_g; colors[..., 2, 2] = c2_b; colors[..., 2, 3] = 255
    
    c3_r = np.where(mask, (r0.astype(np.uint16) + 2*r1)//3, 0).astype(np.uint8)
    c3_g = np.where(mask, (g0.astype(np.uint16) + 2*g1)//3, 0).astype(np.uint8)
    c3_b = np.where(mask, (b0.astype(np.uint16) + 2*b1)//3, 0).astype(np.uint8)
    c3_a = np.where(mask, 255, 0).astype(np.uint8)
    colors[..., 3, 0] = c3_r; colors[..., 3, 1] = c3_g; colors[..., 3, 2] = c3_b; colors[..., 3, 3] = c3_a
    
    idx_bytes = raw[..., 4:8]
    idx32 = (idx_bytes[..., 0].astype(np.uint32) | 
            (idx_bytes[..., 1].astype(np.uint32) << 8) | 
            (idx_bytes[..., 2].astype(np.uint32) << 16) | 
            (idx_bytes[..., 3].astype(np.uint32) << 24))
    
    indices = np.zeros((bh, bw, 4, 4), dtype=np.uint8)
    for row in range(4):
        for col in range(4):
            shift = 2 * (row * 4 + col)
            indices[..., row, col] = ((idx32 >> shift) & 3).astype(np.uint8)
            
    b_idx, w_idx = np.indices((bh, bw))
    out = colors[b_idx[:, :, None, None], w_idx[:, :, None, None], indices]
    out = out.transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return out[:h, :w].copy()


def decode_dxt3(data, w, h):
    """Decompresses DXT3 block data into RGBA8888 numpy array (H, W, 4)."""
    bw = (w + 3) // 4
    bh = (h + 3) // 4
    raw = np.frombuffer(data, dtype=np.uint8).reshape(bh, bw, 16)
    alpha_bytes = raw[..., 0:8]
    color_bytes = raw[..., 8:16]
    
    c0 = color_bytes[..., 0].astype(np.uint16) | (color_bytes[..., 1].astype(np.uint16) << 8)
    c1 = color_bytes[..., 2].astype(np.uint16) | (color_bytes[..., 3].astype(np.uint16) << 8)
    
    r0 = (((c0 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
    g0 = (((c0 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
    b0 = ((c0 & 0x1F) * 255 // 31).astype(np.uint8)
    
    r1 = (((c1 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
    g1 = (((c1 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
    b1 = ((c1 & 0x1F) * 255 // 31).astype(np.uint8)
    
    colors = np.zeros((bh, bw, 4, 3), dtype=np.uint8)
    colors[..., 0, 0] = r0; colors[..., 0, 1] = g0; colors[..., 0, 2] = b0
    colors[..., 1, 0] = r1; colors[..., 1, 1] = g1; colors[..., 1, 2] = b1
    colors[..., 2, 0] = (2*r0.astype(np.uint16) + r1)//3
    colors[..., 2, 1] = (2*g0.astype(np.uint16) + g1)//3
    colors[..., 2, 2] = (2*b0.astype(np.uint16) + b1)//3
    colors[..., 3, 0] = (r0.astype(np.uint16) + 2*r1)//3
    colors[..., 3, 1] = (g0.astype(np.uint16) + 2*g1)//3
    colors[..., 3, 2] = (b0.astype(np.uint16) + 2*b1)//3
    
    idx_bytes = color_bytes[..., 4:8]
    idx32 = (idx_bytes[..., 0].astype(np.uint32) | 
            (idx_bytes[..., 1].astype(np.uint32) << 8) | 
            (idx_bytes[..., 2].astype(np.uint32) << 16) | 
            (idx_bytes[..., 3].astype(np.uint32) << 24))
    
    indices = np.zeros((bh, bw, 4, 4), dtype=np.uint8)
    alphas = np.zeros((bh, bw, 4, 4), dtype=np.uint8)
    
    a16 = alpha_bytes.view(np.uint16).reshape(bh, bw, 4)
    for row in range(4):
        row_a = a16[..., row]
        for col in range(4):
            shift = 2 * (row * 4 + col)
            indices[..., row, col] = ((idx32 >> shift) & 3).astype(np.uint8)
            alphas[..., row, col] = (((row_a >> (col * 4)) & 0xF) * 17).astype(np.uint8)
            
    b_idx, w_idx = np.indices((bh, bw))
    rgb = colors[b_idx[:, :, None, None], w_idx[:, :, None, None], indices]
    out = np.zeros((bh, bw, 4, 4, 4), dtype=np.uint8)
    out[..., :3] = rgb
    out[..., 3] = alphas
    out = out.transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return out[:h, :w].copy()


def decode_dxt5(data, w, h):
    """Decompresses DXT5 block data into RGBA8888 numpy array (H, W, 4)."""
    bw = (w + 3) // 4
    bh = (h + 3) // 4
    raw = np.frombuffer(data, dtype=np.uint8).reshape(bh, bw, 16)
    alpha_bytes = raw[..., 0:8]
    color_bytes = raw[..., 8:16]
    
    a0 = alpha_bytes[..., 0].astype(np.uint16)
    a1 = alpha_bytes[..., 1].astype(np.uint16)
    
    # 8-step alpha table
    atable = np.zeros((bh, bw, 8), dtype=np.uint8)
    atable[..., 0] = a0.astype(np.uint8)
    atable[..., 1] = a1.astype(np.uint8)
    mask = a0 > a1
    for step in range(2, 8):
        if step == 2:
            atable[..., 2] = np.where(mask, (6*a0 + 1*a1)//7, (4*a0 + 1*a1)//5).astype(np.uint8)
        elif step == 3:
            atable[..., 3] = np.where(mask, (5*a0 + 2*a1)//7, (3*a0 + 2*a1)//5).astype(np.uint8)
        elif step == 4:
            atable[..., 4] = np.where(mask, (4*a0 + 3*a1)//7, (2*a0 + 3*a1)//5).astype(np.uint8)
        elif step == 5:
            atable[..., 5] = np.where(mask, (3*a0 + 4*a1)//7, (1*a0 + 4*a1)//5).astype(np.uint8)
        elif step == 6:
            atable[..., 6] = np.where(mask, (2*a0 + 5*a1)//7, 0).astype(np.uint8)
        elif step == 7:
            atable[..., 7] = np.where(mask, (1*a0 + 6*a1)//7, 255).astype(np.uint8)
            
    # Alpha 48-bit indices (16 x 3-bit)
    abits = np.zeros((bh, bw), dtype=np.uint64)
    for b in range(6):
        abits |= alpha_bytes[..., 2 + b].astype(np.uint64) << (b * 8)
        
    alphas = np.zeros((bh, bw, 4, 4), dtype=np.uint8)
    b_idx, w_idx = np.indices((bh, bw))
    for row in range(4):
        for col in range(4):
            shift = 3 * (row * 4 + col)
            a_idx = ((abits >> shift) & 7).astype(np.uint8)
            alphas[..., row, col] = atable[b_idx, w_idx, a_idx]
            
    # Color
    c0 = color_bytes[..., 0].astype(np.uint16) | (color_bytes[..., 1].astype(np.uint16) << 8)
    c1 = color_bytes[..., 2].astype(np.uint16) | (color_bytes[..., 3].astype(np.uint16) << 8)
    
    r0 = (((c0 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
    g0 = (((c0 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
    b0 = ((c0 & 0x1F) * 255 // 31).astype(np.uint8)
    
    r1 = (((c1 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
    g1 = (((c1 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
    b1 = ((c1 & 0x1F) * 255 // 31).astype(np.uint8)
    
    colors = np.zeros((bh, bw, 4, 3), dtype=np.uint8)
    colors[..., 0, 0] = r0; colors[..., 0, 1] = g0; colors[..., 0, 2] = b0
    colors[..., 1, 0] = r1; colors[..., 1, 1] = g1; colors[..., 1, 2] = b1
    colors[..., 2, 0] = (2*r0.astype(np.uint16) + r1)//3
    colors[..., 2, 1] = (2*g0.astype(np.uint16) + g1)//3
    colors[..., 2, 2] = (2*b0.astype(np.uint16) + b1)//3
    colors[..., 3, 0] = (r0.astype(np.uint16) + 2*r1)//3
    colors[..., 3, 1] = (g0.astype(np.uint16) + 2*g1)//3
    colors[..., 3, 2] = (b0.astype(np.uint16) + 2*b1)//3
    
    idx_bytes = color_bytes[..., 4:8]
    idx32 = (idx_bytes[..., 0].astype(np.uint32) | 
            (idx_bytes[..., 1].astype(np.uint32) << 8) | 
            (idx_bytes[..., 2].astype(np.uint32) << 16) | 
            (idx_bytes[..., 3].astype(np.uint32) << 24))
    
    indices = np.zeros((bh, bw, 4, 4), dtype=np.uint8)
    for row in range(4):
        for col in range(4):
            shift = 2 * (row * 4 + col)
            indices[..., row, col] = ((idx32 >> shift) & 3).astype(np.uint8)
            
    rgb = colors[b_idx[:, :, None, None], w_idx[:, :, None, None], indices]
    out = np.zeros((bh, bw, 4, 4, 4), dtype=np.uint8)
    out[..., :3] = rgb
    out[..., 3] = alphas
    out = out.transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return out[:h, :w].copy()


def decode_raw(tex, level=0):
    """Decodes uncompressed texture data into RGBA8888 (H, W, 4)."""
    data = tex.mip_data[level]
    w = max(1, tex.width >> level)
    h = max(1, tex.height >> level)
    fmt = tex.format & 0xF00
    
    if tex.format & RW_RASTER_PAL8 and tex.palette:
        pal = np.frombuffer(tex.palette, dtype=np.uint8).reshape(-1, 4) # RGBA
        idx = np.frombuffer(data, dtype=np.uint8)[:w*h]
        return pal[idx].reshape(h, w, 4).copy()
    
    if tex.format & RW_RASTER_PAL4 and tex.palette:
        pal = np.frombuffer(tex.palette, dtype=np.uint8).reshape(-1, 4)
        raw_b = np.frombuffer(data, dtype=np.uint8)
        idx = np.zeros(w * h, dtype=np.uint8)
        for i in range(w * h):
            b = raw_b[i // 2]
            idx[i] = (b & 0xF) if (i % 2 == 0) else ((b >> 4) & 0xF)
        return pal[idx].reshape(h, w, 4).copy()
    
    if tex.depth == 32 or fmt == RW_RASTER_C8888:
        # D3D8: BGRA8888
        arr = np.frombuffer(data, dtype=np.uint8)[:w*h*4].reshape(h, w, 4)
        return arr[..., [2, 1, 0, 3]].copy()
        
    if tex.depth == 24 or fmt == RW_RASTER_C888:
        # D3D8: BGR888
        arr = np.frombuffer(data, dtype=np.uint8)[:w*h*3].reshape(h, w, 3)
        rgba = np.full((h, w, 4), 255, dtype=np.uint8)
        rgba[..., 0] = arr[..., 2] # R
        rgba[..., 1] = arr[..., 1] # G
        rgba[..., 2] = arr[..., 0] # B
        return rgba
        
    if tex.depth == 16:
        raw16 = np.frombuffer(data, dtype=np.uint16)[:w*h].reshape(h, w)
        rgba = np.zeros((h, w, 4), dtype=np.uint8)
        if fmt == RW_RASTER_C1555:
            rgba[..., 0] = (((raw16 >> 10) & 0x1F) * 255 // 31).astype(np.uint8)
            rgba[..., 1] = (((raw16 >> 5) & 0x1F) * 255 // 31).astype(np.uint8)
            rgba[..., 2] = ((raw16 & 0x1F) * 255 // 31).astype(np.uint8)
            rgba[..., 3] = np.where((raw16 & 0x8000) != 0, 255, 0).astype(np.uint8)
        elif fmt == RW_RASTER_C4444:
            rgba[..., 0] = (((raw16 >> 8) & 0xF) * 17).astype(np.uint8)
            rgba[..., 1] = (((raw16 >> 4) & 0xF) * 17).astype(np.uint8)
            rgba[..., 2] = ((raw16 & 0xF) * 17).astype(np.uint8)
            rgba[..., 3] = (((raw16 >> 12) & 0xF) * 17).astype(np.uint8)
        else: # C565 default
            rgba[..., 0] = (((raw16 >> 11) & 0x1F) * 255 // 31).astype(np.uint8)
            rgba[..., 1] = (((raw16 >> 5) & 0x3F) * 255 // 63).astype(np.uint8)
            rgba[..., 2] = ((raw16 & 0x1F) * 255 // 31).astype(np.uint8)
            rgba[..., 3] = 255
        return rgba
        
    raise ValueError(f"Unsupported uncompressed format: depth={tex.depth}, format=0x{tex.format:x}")


def decode_texture(tex, level=0):
    """Decodes any supported RWTexture mip level into an RGBA8888 numpy array."""
    data = tex.mip_data[level]
    w = max(1, tex.width >> level)
    h = max(1, tex.height >> level)
    
    if tex.compression == 1:
        return decode_dxt1(data, w, h)
    elif tex.compression == 3:
        return decode_dxt3(data, w, h)
    elif tex.compression == 5:
        return decode_dxt5(data, w, h)
    elif tex.compression == 0:
        return decode_raw(tex, level)
    else:
        raise ValueError(f"Unsupported compression type: {tex.compression}")


# ---------------------------------------------------------------------------
# Target Format Encoders (RGB565, RGBA4444, RGBA5551, PAL8)
# ---------------------------------------------------------------------------

def encode_rgb565(rgba):
    """Packs RGBA8888 array into 16bpp RGB565 little-endian bytes."""
    r = (rgba[..., 0] >> 3).astype(np.uint16)
    g = (rgba[..., 1] >> 2).astype(np.uint16)
    b = (rgba[..., 2] >> 3).astype(np.uint16)
    val = (r << 11) | (g << 5) | b
    return val.tobytes()


def encode_rgba4444(rgba):
    """Packs RGBA8888 array into 16bpp RGBA4444 little-endian bytes."""
    r = (rgba[..., 0] >> 4).astype(np.uint16)
    g = (rgba[..., 1] >> 4).astype(np.uint16)
    b = (rgba[..., 2] >> 4).astype(np.uint16)
    a = (rgba[..., 3] >> 4).astype(np.uint16)
    val = (r << 12) | (g << 8) | (b << 4) | a
    return val.tobytes()


def encode_rgba5551(rgba):
    """Packs RGBA8888 array into 16bpp RGBA5551 little-endian bytes."""
    r = (rgba[..., 0] >> 3).astype(np.uint16)
    g = (rgba[..., 1] >> 3).astype(np.uint16)
    b = (rgba[..., 2] >> 3).astype(np.uint16)
    a = (rgba[..., 3] >> 7).astype(np.uint16)
    val = (r << 11) | (g << 6) | (b << 1) | a
    return val.tobytes()


def quantize_rgba_median_cut(pixels_rgba, max_colors=256):
    """
    Quantizes an RGBA image to max_colors (256) entries with alpha channel.
    Returns:
      palette: (max_colors, 4) uint8 RGBA array
      indices: (N,) uint8 palette index array
    """
    flat = pixels_rgba.reshape(-1, 4)
    flat_u32 = flat.view(np.uint32).reshape(-1)
    unique_u32, inv, counts = np.unique(flat_u32, return_inverse=True, return_counts=True)
    
    if len(unique_u32) <= max_colors:
        palette = np.zeros((max_colors, 4), dtype=np.uint8)
        unique_rgba = unique_u32.view(np.uint8).reshape(-1, 4)
        palette[:len(unique_rgba)] = unique_rgba
        return palette, inv.astype(np.uint8)
    
    unique_rgba = unique_u32.view(np.uint8).reshape(-1, 4).astype(np.float32)
    boxes = [(unique_rgba, counts)]
    
    while len(boxes) < max_colors:
        best_idx = -1
        best_spread = -1
        best_axis = -1
        for i, (pts, w) in enumerate(boxes):
            if len(pts) <= 1:
                continue
            spread = pts.max(axis=0) - pts.min(axis=0)
            axis = np.argmax(spread)
            if spread[axis] > best_spread:
                best_spread = spread[axis]
                best_idx = i
                best_axis = axis
        if best_idx == -1 or best_spread == 0:
            break
            
        pts, w = boxes.pop(best_idx)
        sort_order = np.argsort(pts[:, best_axis])
        pts = pts[sort_order]
        w = w[sort_order]
        cum_w = np.cumsum(w)
        median_idx = np.searchsorted(cum_w, cum_w[-1] / 2)
        median_idx = max(1, min(len(pts) - 1, median_idx))
        boxes.append((pts[:median_idx], w[:median_idx]))
        boxes.append((pts[median_idx:], w[median_idx:]))
        
    palette = np.zeros((max_colors, 4), dtype=np.uint8)
    centroids = []
    for i, (pts, w) in enumerate(boxes):
        center = np.average(pts, axis=0, weights=w)
        palette[i] = np.clip(np.round(center), 0, 255).astype(np.uint8)
        centroids.append(center)
        
    centroids = np.array(centroids, dtype=np.float32)
    chunk_size = 16384
    indices = np.zeros(len(flat), dtype=np.uint8)
    for c in range(0, len(flat), chunk_size):
        chunk = flat[c:c+chunk_size].astype(np.float32)
        diff = chunk[:, None, :] - centroids[None, :, :]
        dist_sq = np.sum(diff * diff, axis=2)
        indices[c:c+chunk_size] = np.argmin(dist_sq, axis=1)
        
    return palette, indices


def match_to_palette(rgba_pixels, palette):
    """Maps RGBA pixels to the closest entry in an existing palette."""
    flat = rgba_pixels.reshape(-1, 4).astype(np.float32)
    pal = palette.astype(np.float32)
    chunk_size = 16384
    indices = np.zeros(len(flat), dtype=np.uint8)
    for c in range(0, len(flat), chunk_size):
        chunk = flat[c:c+chunk_size]
        diff = chunk[:, None, :] - pal[None, :, :]
        dist_sq = np.sum(diff * diff, axis=2)
        indices[c:c+chunk_size] = np.argmin(dist_sq, axis=1)
    return indices


# ---------------------------------------------------------------------------
# Mipmap Generator
# ---------------------------------------------------------------------------

def encode_pal8_16(pixels, alpha=False):
    """GLES PAL8: 512-byte RGB565/RGBA4 palette then row-major indices.

    Images with <=256 colours stay lossless relative to the packed LE input.
    """
    words = np.frombuffer(pixels, dtype='<u2')
    unique, inverse = np.unique(words, return_inverse=True)
    if len(unique) <= 256:
        palette = np.zeros(256, dtype='<u2')
        palette[:len(unique)] = unique
        return palette.tobytes() + inverse.astype(np.uint8).tobytes()
    rgba = np.empty((len(words), 1, 4), dtype=np.uint8)
    if alpha:
        for i, shift in enumerate((12, 8, 4, 0)):
            rgba[:, 0, i] = ((words >> shift) & 15) * 17
    else:
        for i, shift, mask in ((0, 11, 31), (1, 5, 63), (2, 0, 31)):
            rgba[:, 0, i] = ((words >> shift) & mask) * 255 // mask
        rgba[:, 0, 3] = 255
    palette, indices = quantize_rgba_median_cut(rgba)
    packed = encode_rgba4444(palette) if alpha else encode_rgb565(palette)
    return packed + indices.tobytes()


def generate_mip_chain(base_rgba, max_mips=NANTEX_MAX_MIPS):
    """Generates a full mip chain down to 1x1 from an RGBA8888 base image."""
    h, w = base_rgba.shape[:2]
    cur_img = Image.fromarray(base_rgba, "RGBA")
    mips = [base_rgba]
    
    cur_w, cur_h = w, h
    while (cur_w > 1 or cur_h > 1) and len(mips) < max_mips:
        cur_w = max(1, cur_w // 2)
        cur_h = max(1, cur_h // 2)
        resized = cur_img.resize((cur_w, cur_h), RESAMPLE_BILINEAR)
        mips.append(np.frombuffer(resized.tobytes(), dtype=np.uint8).reshape(cur_h, cur_w, 4).copy())
        
    return mips


# ---------------------------------------------------------------------------
# .nantex Container Writer & Validator
# ---------------------------------------------------------------------------

class NanTexPacker:
    def __init__(self):
        self.textures = [] # list of dicts

    def add_texture(self, name, width, height, target_format, filter_flags, mip_buffers, palette_bytes=None):
        self.textures.append({
            "name": name,
            "width": width,
            "height": height,
            "format": target_format,
            "filter_flags": filter_flags,
            "mip_buffers": mip_buffers,
            "palette_bytes": palette_bytes
        })

    def write(self, out_path):
        count = len(self.textures)
        header_size = 16
        entry_size = 156
        table_size = count * entry_size
        
        # We first build the entries and data section
        data_payload = bytearray()
        entries_data = bytearray()
        
        current_data_offset = header_size + table_size
        
        for tex in self.textures:
            # 4-byte align payload
            padding = (4 - (len(data_payload) % 4)) % 4
            if padding:
                data_payload.extend(b"\x00" * padding)
                
            tex_offset = current_data_offset + len(data_payload)
            tex_data_start = len(data_payload)
            
            pal_offset = 0
            pal_size = 0
            if tex["palette_bytes"]:
                pal_offset = len(data_payload) - tex_data_start
                pal_size = len(tex["palette_bytes"])
                data_payload.extend(tex["palette_bytes"])
                
            mip_count = len(tex["mip_buffers"])
            mip_offsets = [0] * NANTEX_MAX_MIPS
            mip_sizes = [0] * NANTEX_MAX_MIPS
            
            for m, mbuf in enumerate(tex["mip_buffers"]):
                mip_offsets[m] = len(data_payload) - tex_data_start
                mip_sizes[m] = len(mbuf)
                data_payload.extend(mbuf)
                
            tex_data_size = len(data_payload) - tex_data_start
            
            # Pack entry (156 bytes)
            name_bytes = tex["name"].encode("latin1")[:31].ljust(32, b"\x00")
            entry_bytes = struct.pack(
                "<32sHHHHIIIII12I12I",
                name_bytes,
                tex["width"],
                tex["height"],
                tex["format"],
                mip_count,
                tex["filter_flags"],
                tex_offset,
                tex_data_size,
                pal_offset,
                pal_size,
                *mip_offsets,
                *mip_sizes
            )
            assert len(entry_bytes) == entry_size, f"Entry size {len(entry_bytes)} != {entry_size}"
            entries_data.extend(entry_bytes)
            
        header_bytes = struct.pack(
            "<4sHHII",
            NANTEX_MAGIC,
            NANTEX_VERSION,
            0, # flags
            count,
            0  # reserved
        )
        assert len(header_bytes) == header_size
        
        with open(out_path, "wb") as f:
            f.write(header_bytes)
            f.write(entries_data)
            f.write(data_payload)


def validate_nantex(path):
    """Validates the structure, headers, offsets, and mip data of a .nantex container."""
    file_size = os.path.getsize(path)
    with open(path, "rb") as f:
        if file_size < 16:
            raise ValueError(f"File too small for NanTex header: {file_size} bytes")
            
        magic, version, flags, count, reserved = struct.unpack("<4sHHII", f.read(16))
        if magic != NANTEX_MAGIC:
            raise ValueError(f"Invalid magic: expected {NANTEX_MAGIC}, got {magic}")
        if version != NANTEX_VERSION:
            raise ValueError(f"Unsupported version: {version}")
            
        table_size = count * 156
        if 16 + table_size > file_size:
            raise ValueError(f"File truncated: header + entries {16 + table_size} > file size {file_size}")
            
        entries = []
        for i in range(count):
            entry_raw = f.read(156)
            unpacked = struct.unpack("<32sHHHHIIIII12I12I", entry_raw)
            name = unpacked[0].split(b"\x00")[0].decode("latin1", errors="replace")
            w, h, fmt, mips, filter_f, data_off, data_sz, pal_off, pal_sz = unpacked[1:10]
            mip_offs = unpacked[10:22]
            mip_sizes = unpacked[22:34]
            
            if not name:
                raise ValueError(f"Texture {i} has empty name")
            if w == 0 or h == 0:
                raise ValueError(f"Texture {name} has zero dimension: {w}x{h}")
            if fmt not in FMT_NAMES:
                raise ValueError(f"Texture {name} has invalid format ID: {fmt}")
            if mips == 0 or mips > NANTEX_MAX_MIPS:
                raise ValueError(f"Texture {name} has invalid mip count: {mips}")
            if data_off + data_sz > file_size:
                raise ValueError(f"Texture {name} payload exceeds file boundary: {data_off + data_sz} > {file_size}")
            if pal_sz > 0 and pal_off + pal_sz > data_sz:
                raise ValueError(f"Texture {name} palette exceeds texture payload")
                
            cur_w, cur_h = w, h
            for m in range(mips):
                moff = mip_offs[m]
                msz = mip_sizes[m]
                if moff + msz > data_sz:
                    raise ValueError(f"Texture {name} mip {m} exceeds payload: {moff + msz} > {data_sz}")
                
                # Check expected byte length
                if fmt in (NANTEX_FMT_RGB565, NANTEX_FMT_RGBA4444, NANTEX_FMT_RGBA5551):
                    expected = cur_w * cur_h * 2
                elif fmt in (NANTEX_FMT_PAL8_RGBA8888, NANTEX_FMT_PAL8_RGBA4444, NANTEX_FMT_PAL8_RGB565):
                    expected = cur_w * cur_h * 1
                else:
                    expected = msz
                    
                if msz != expected:
                    raise ValueError(f"Texture {name} mip {m} size mismatch: got {msz}, expected {expected}")
                    
                cur_w = max(1, cur_w // 2)
                cur_h = max(1, cur_h // 2)
                
            entries.append((name, w, h, fmt, mips, data_sz))
            
    return count, entries


# ---------------------------------------------------------------------------
# CLI Actions: --info, Convert, --validate
# ---------------------------------------------------------------------------

def run_info(txd_path):
    """Dumps detailed content information of a .txd archive."""
    print(f"=== TXD Archive Info: {txd_path} ===")
    textures = parse_txd(txd_path)
    print(f"Total Textures: {len(textures)}")
    print("-" * 88)
    print(f"{'#':<3} {'Name':<20} {'Size':<10} {'Depth':<6} {'Mips':<5} {'Comp':<7} {'Alpha':<6} {'Raw Bytes':<10}")
    print("-" * 88)
    
    total_raw = 0
    for i, tex in enumerate(textures):
        comp_str = {0: "Raw", 1: "DXT1", 2: "DXT2", 3: "DXT3", 4: "DXT4", 5: "DXT5"}.get(tex.compression, f"?({tex.compression})")
        raw_sz = sum(len(m) for m in tex.mip_data)
        if tex.palette:
            raw_sz += len(tex.palette)
        total_raw += raw_sz
        alpha_str = "Yes" if tex.has_alpha else "No"
        print(f"{i:<3} {tex.name:<20} {f'{tex.width}x{tex.height}':<10} {tex.depth:<6} {tex.num_levels:<5} {comp_str:<7} {alpha_str:<6} {raw_sz:<10}")
        
    print("-" * 88)
    print(f"Archive File Size: {os.path.getsize(txd_path):,} bytes | Total Payload: {total_raw:,} bytes\n")


def convert_txd(txd_path, out_path, alpha_mode="auto", mips_mode="full", max_dim=None, verbose=True, texture_mode="native"):
    """Converts a TXD file to a .nantex container."""
    orig_size = os.path.getsize(txd_path)
    textures = parse_txd(txd_path)
    
    packer = NanTexPacker()
    
    if verbose:
        print(f"\nProcessing '{os.path.basename(txd_path)}' ({len(textures)} textures)...")
        print(f"{'Name':<20} {'Original':<16} {'Target Format':<20} {'Mips':<5} {'Nantex Size':<12}")
        print("-" * 78)
        
    for tex in textures:
        base_rgba = decode_texture(tex, 0)
        
        # Optional downscaling
        if max_dim and (base_rgba.shape[1] > max_dim or base_rgba.shape[0] > max_dim):
            cur_w, cur_h = base_rgba.shape[1], base_rgba.shape[0]
            scale = max_dim / max(cur_w, cur_h)
            new_w = max(1, int(round(cur_w * scale)))
            new_h = max(1, int(round(cur_h * scale)))
            pil_img = Image.fromarray(base_rgba, "RGBA").resize((new_w, new_h), RESAMPLE_BILINEAR)
            base_rgba = np.frombuffer(pil_img.tobytes(), dtype=np.uint8).reshape(new_h, new_w, 4).copy()
            
        cur_h, cur_w = base_rgba.shape[:2]
        
        # Check actual transparency
        has_transparency = np.any(base_rgba[..., 3] < 255)
        
        # Decide format
        target_fmt = NANTEX_FMT_RGB565
        if has_transparency:
            flat_u32 = base_rgba.reshape(-1, 4).view(np.uint32)
            unique_colors = len(np.unique(flat_u32))
            
            if alpha_mode == "4444":
                target_fmt = NANTEX_FMT_RGBA4444
            elif alpha_mode == "pal8":
                target_fmt = NANTEX_FMT_PAL8_RGBA8888
            elif alpha_mode == "5551":
                target_fmt = NANTEX_FMT_RGBA5551
            else: # auto
                if unique_colors <= 256:
                    target_fmt = NANTEX_FMT_PAL8_RGBA8888
                else:
                    target_fmt = NANTEX_FMT_RGBA4444
                    
        if texture_mode == "pal8":
            target_fmt = NANTEX_FMT_PAL8_RGBA4444 if has_transparency else NANTEX_FMT_PAL8_RGB565

        # Generate or collect mip levels
        if mips_mode == "full":
            mip_chain = generate_mip_chain(base_rgba)
        elif mips_mode == "keep":
            mip_chain = [base_rgba]
            for m in range(1, tex.num_levels):
                mip_chain.append(decode_texture(tex, m))
        else: # none
            mip_chain = [base_rgba]
            
        mip_buffers = []
        palette_bytes = None
        
        if target_fmt in (NANTEX_FMT_PAL8_RGBA4444, NANTEX_FMT_PAL8_RGB565):
            alpha = target_fmt == NANTEX_FMT_PAL8_RGBA4444
            packed = encode_rgba4444(base_rgba) if alpha else encode_rgb565(base_rgba)
            payload = encode_pal8_16(packed, alpha)
            palette_bytes = payload[:512]
            mip_buffers.append(payload[512:])
            words = np.frombuffer(palette_bytes, dtype='<u2')
            pal_rgba = np.full((256, 4), 255, dtype=np.uint8)
            channels = ((0,12,15),(1,8,15),(2,4,15),(3,0,15)) if alpha else ((0,11,31),(1,5,63),(2,0,31))
            for channel, shift, mask in channels:
                pal_rgba[:, channel] = ((words >> shift) & mask) * 255 // mask
            for mip in mip_chain[1:]:
                mip_buffers.append(match_to_palette(mip, pal_rgba).tobytes())

        elif target_fmt == NANTEX_FMT_PAL8_RGBA8888:
            # Generate 256-color palette from base level
            pal_rgba, base_indices = quantize_rgba_median_cut(base_rgba, 256)
            palette_bytes = pal_rgba.tobytes()
            mip_buffers.append(base_indices.tobytes())
            
            # Map lower mips to this palette
            for m in range(1, len(mip_chain)):
                m_indices = match_to_palette(mip_chain[m], pal_rgba)
                mip_buffers.append(m_indices.tobytes())
                
        elif target_fmt == NANTEX_FMT_RGB565:
            for m_rgba in mip_chain:
                mip_buffers.append(encode_rgb565(m_rgba))
        elif target_fmt == NANTEX_FMT_RGBA4444:
            for m_rgba in mip_chain:
                mip_buffers.append(encode_rgba4444(m_rgba))
        elif target_fmt == NANTEX_FMT_RGBA5551:
            for m_rgba in mip_chain:
                mip_buffers.append(encode_rgba5551(m_rgba))
                
        tex_bytes = sum(len(b) for b in mip_buffers) + (len(palette_bytes) if palette_bytes else 0)
        packer.add_texture(tex.name, cur_w, cur_h, target_fmt, tex.filter_flags, mip_buffers, palette_bytes)
        
        if verbose:
            comp_name = {0:"Raw", 1:"DXT1", 3:"DXT3", 5:"DXT5"}.get(tex.compression, str(tex.compression))
            orig_desc = f"{tex.width}x{tex.height} {comp_name}"
            print(f"{tex.name:<20} {orig_desc:<16} {FMT_NAMES[target_fmt]:<20} {len(mip_buffers):<5} {tex_bytes:<12,}")
            
    packer.write(out_path)
    
    # Auto-validate the output file
    valid_count, _ = validate_nantex(out_path)
    conv_size = os.path.getsize(out_path)
    ratio = (conv_size / orig_size) * 100.0
    
    if verbose:
        print("-" * 78)
        print(f"Validation: Verified {valid_count} textures in '{os.path.basename(out_path)}' (Offsets & sizes OK)")
        print(f"Summary: Original: {orig_size:,} bytes -> Converted: {conv_size:,} bytes ({ratio:.1f}%)\n")
        
    return orig_size, conv_size, len(textures)


# ---------------------------------------------------------------------------
# Main Entry Point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="txdconv - Convert Vice City PC TXD archives to ViceNano .nantex containers"
    )
    parser.add_argument("input", nargs="?", help="Input .txd file or .nantex file (for --validate)")
    parser.add_argument("output", nargs="?", help="Output .nantex file path (optional)")
    parser.add_argument("--info", action="store_true", help="Dump contents of input .txd file")
    parser.add_argument("--validate", action="store_true", help="Validate an existing .nantex file")
    parser.add_argument("--alpha-format", choices=["auto", "4444", "pal8", "5551"], default="auto",
                        help="Format for textures with alpha (default: auto -> PAL8 if <=256 colors, else 4444)")
    parser.add_argument("--texture-format", choices=["native", "pal8"], default="native",
                        help="PAL8 uses RGB565/RGBA4 palettes for opaque/alpha textures")
    parser.add_argument("--mips", choices=["full", "keep", "none"], default="full",
                        help="Mipmap strategy (default: full -> mip chain to 1x1 for MBX min-filters)")
    parser.add_argument("--max-dim", type=int, default=None,
                        help="Downscale textures larger than max-dim (e.g. 128, 256)")
    
    args = parser.parse_args()
    
    if not args.input:
        parser.print_help()
        sys.exit(1)
        
    if args.validate:
        try:
            count, entries = validate_nantex(args.input)
            print(f"SUCCESS: Validated '{args.input}'")
            print(f"Header magic: 'NANT', Version: 1, Texture Count: {count}")
            for name, w, h, fmt, mips, sz in entries:
                print(f"  {name:<20} {w}x{h:<8} {FMT_NAMES.get(fmt, str(fmt)):<18} mips={mips:<2} {sz:,} bytes")
            print(f"All {count} textures and offsets verified successfully.")
            sys.exit(0)
        except Exception as e:
            print(f"ERROR: Validation failed for '{args.input}': {e}", file=sys.stderr)
            sys.exit(1)
            
    if args.info:
        try:
            run_info(args.input)
            sys.exit(0)
        except Exception as e:
            print(f"ERROR: Failed to read TXD info: {e}", file=sys.stderr)
            sys.exit(1)
            
    # Default: Convert TXD to .nantex
    out_file = args.output
    if not out_file:
        base, _ = os.path.splitext(args.input)
        out_file = base + ".nantex"
        
    try:
        convert_txd(
            args.input,
            out_file,
            alpha_mode=args.alpha_format,
            mips_mode=args.mips,
            max_dim=args.max_dim,
            texture_mode=args.texture_format
        )
    except Exception as e:
        print(f"ERROR during conversion: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
