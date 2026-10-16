#pragma once
#include <stdint.h>
struct NanVertex { float x,y,z,u,v; uint8_t r,g,b,a; };
struct M8Vertex { float x, y, z; uint8_t r, g, b, a; float u, v; };
struct NanTexture {
 int width,height,format; // Original 16-bit format; also controls alpha state.
 const uint8_t *pixels;
 const char *name;
 uint32_t bytes,compressedFormat;
};
struct NanPart { int vertices,triangles,texture; const NanVertex *data; const uint16_t *indices; };

struct NanPack { const NanTexture *textures; const NanPart *parts; unsigned numTextures,numParts; unsigned reservation; float x,y,z; int blockX,blockZ; };
struct M8RenderStats {
 uint32_t texturePackedBytes, textureResidentBytes, textureFallbacks, textureUploads;
 uint32_t drawCalls, vertices, indices;
};
extern M8RenderStats m8Stats;
void unloadBlock(unsigned slot);
bool loadBlock(unsigned slot,const NanPack &pack);
bool m8PrepareBatches();
void renderWorld();
void m8ResetRenderState();

#ifdef M8_NANOOS
bool m8NanoModelRender(int pan);
#endif
