1. TEXTURE INVENTORY (GTA Vice City PC 1.0; 1,377 TXDs total, 12,275 textures):
   - Loose TXD (9 files, 2.32 MB / 2,428,352 B): fronten2 (748 KB, 24 tex), fonts (524 KB, 2 tex), particle (454 KB, 116 tex), generic (262 KB), fronten1 (198 KB), hud (107 KB).
   - gta3.img TXD (1,368 files, 174.10 MB / 182,556,672 B): bb_barroom (1,312 KB, 56 tex), lhaitiblk6f (1,310 KB, 45 tex), bb_barroomtrans (1,252 KB), funhiati (1,162 KB), dt_shops13 (1,000 KB).
   - Dominant Formats: DXT1 (11,004 tex, 89.6%, 148.7 MB on disk; 10,792 RGB565 + 212 RGBA1555), DXT3 (1,241 tex, 10.1%, 24.3 MB; RGBA4444), PAL8 (27 tex, 0.2%, 0.25 MB), Raw 32bpp (3 tex).
   - Dimensions: 128x128 (3,804 tex, 31.0%), 256x256 (3,575 tex, 29.1%, 71.3% of raw bytes), 64x64 (3,288 tex, 26.8%), 32x32 (745 tex, 6.1%). Max: 3x 512x512, 3x 256x512.
2. MODEL & WORLD DATA SIZES:
   - gta3.img (6,043 files, 312.32 MB total): .txd 1,368 (174.10 MB, 55.7%), .dff 4,617 (130.15 MB, 41.7%), .col 30 (6.36 MB, 2.0%), .ifp 28 (1.70 MB, 0.5%).
   - data/maps/: 33 .ipl (2.07 MB), 30 .col (6.33 MB), 33 .ide (0.31 MB). Models coll: 4 .col (212 KB: vehicles 104 KB, generic 83 KB, peds 21 KB).
   - anim/cuts.img: 109.75 MB (76 cutscene .ifp @ 109.46 MB, 72 .dat @ 0.30 MB). Main ped animation: anim/ped.ifp (2.10 MB).
   - Largest .dff models: IslandLODbeach (360 KB), IslandLODmainland (338 KB), od_lightint (334 KB), buildingsite2 (330 KB). Average .dff: 28.2 KB.
3. CONVERSION STRATEGY (~24-32MB TEXTURE BUDGET IN 64MB RAM):
   - MBX GLES1 Native: GL_RGB/565 (16bpp), GL_RGBA/4444 & 5551 (16bpp), GL_OES_compressed_paletted_texture (8bpp PAL8, 1 B/px), PVRTC 4bpp (if driver exposes). NO native DXT.
   - Downscaling: 512x512->256x256 (UI/fonts/background; iPod screen is 240x432); 256x256->128x128 (world/buildings/cars; 71.3% of raw bytes, drops footprint by 75%).
   - Palettization: 32bpp/16bpp alpha textures -> 8bpp paletted (GL_PALETTE8_RGBA4/RGBA8_OES) for decals, particles, and foliage (50% saving vs 16bpp).
   - Expected Final Size (All 12,275 textures on disk / in VRAM):
     * Option A (16bpp max 128x128): 269.3 MB full archive on NAND; 24-32MB streaming RAM window holds 768-1,024 concurrent textures (32 KB/tex).
     * Option B (8bpp Paletted max 128x128): 146.6 MB full archive on NAND; 24-32MB streaming RAM window holds 1,536-2,048 concurrent textures (16 KB/tex).
     * Option C (PVRTC 4bpp / 1/2 downscale): 39.2 MB full archive on NAND; 24-32MB streaming RAM window holds 3,000+ concurrent textures (8 KB/tex).
4. ASSETS TOO LARGE TO FIT (FLAGGED):
   - anim/cuts.img: law_1b.ifp (6.38 MB), cub_1.ifp (3.67 MB), tax_1.ifp (3.09 MB), law_1a.ifp (3.04 MB) — single animations exceed 5-10% of total DRAM; stream or strip cutscenes.
   - movies/GTAtitles.mpg (24.0 MB) & Logo.mpg (7.0 MB) — video files cannot fit memory; must be bypassed or decoded via AppleM2 HW block to framebuffer.
   - TXD VRAM blowouts: bb_barroom.txd (1.31 MB DXT disk -> 4.33 MB in 16bpp / 8.66 MB in 32bpp VRAM native; 56 tex), lhaitiblk6f.txd (4.1 MB VRAM), bb_barroomtrans.txd (3.8 MB VRAM).
   - Single textures: fonts.txd (font1/font2 512x512 @ 512 KB in 16bpp / 1 MB in 32bpp each) & fronten1.txd (background 512 KB) — must halve to 256x256 (128 KB 16bpp).
