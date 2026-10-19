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
// View: eye x,y,z; heading sine/cosine (forward = (s,0,c)); pitch
// cosine/sine (ps > 0 looks down); the eye sits back units behind it along
// the view; near clip plane; cull = drop back faces (screen winding).
struct M8Cam { float x,y,z,s,c,pc,ps,back,near; bool cull; };
bool m8NanoModelRender(const M8Cam &cam);
// NVW3 walk scene (tools/make_vc_world.py, 1/16 m): models drawn whole at
// one of two LODs (first/count each), a ground-height grid for the eye, the
// triangles; textures in block 0
struct VcModel { int16_t x,y,z,r; uint16_t first0,count0,first1,count1; };
struct VcGround { int16_t x,z,h; };
struct VcTri { int16_t xyz[9],uv[6]; uint8_t rgb[9],tex; };
struct M8VcScene { const VcModel *models; const VcGround *grounds; const VcTri *tris; unsigned numModels,numGrounds; };
extern M8VcScene m8Vc;
float m8VcGround(float x,float z,float fallback);
#endif
