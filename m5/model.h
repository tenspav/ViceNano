#pragma once
#include <stdint.h>
struct NanVertex { float x,y,z,u,v; uint8_t r,g,b,a; };
struct NanTexture { int width,height,format; const uint16_t *pixels; const char *name; };
struct NanPart { int vertices,triangles,texture; const NanVertex *data; const uint16_t *indices; };

struct NanPack { const NanTexture *textures; const NanPart *parts; unsigned numTextures,numParts; unsigned reservation; float x,y,z; int blockX,blockZ; };
void unloadBlock(unsigned slot);
bool loadBlock(unsigned slot,const NanPack &pack);
void renderBlock(unsigned slot);
