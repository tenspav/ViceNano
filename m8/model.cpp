#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
#include "../vendor/librw/src/gles1/rwgles1.h"
#include <stddef.h>
#include <string.h>

extern "C" {
#include "gl.h"
}

extern RpAtomic *GetFirstAtomic(RpClump*);
extern RwTexture *GetFirstTexture(RwTexDictionary*);
#include "model.h"

using namespace rw;

namespace rw { namespace gles1 {
struct MeshInstance { Im3DVertex *vertices; bool alpha; };
struct Header : rw::InstanceDataHeader { uint16 serial; uint16 count; MeshInstance meshes[1]; };
}}

static void m8Instance(ObjPipeline*, Atomic *a) {
 Geometry *g = a->geometry;
 if ((g->flags & Geometry::NATIVE) || g->numVertices < 1 || g->numVertices > 65535 || !g->morphTargets[0].vertices) {
  gles1::unsupportedStates++;
  return;
 }
 if (!g->meshHeader) g->buildMeshes();
 if (g->instData) {
  gles1::Header *h = (gles1::Header*)g->instData;
  if (h->platform == PLATFORM_GLES1 && h->serial == g->meshHeader->serialNum && !g->lockedSinceInst)
   return;
  gles1::freeInstanceData(g);
 }
 unsigned count = g->meshHeader->numMeshes;
 if (count < 1 || count > 64) {
  gles1::unsupportedStates++;
  return;
 }
 gles1::Header *h = (gles1::Header*)rwMalloc(sizeof(gles1::Header) + (count - 1) * sizeof(gles1::MeshInstance), MEMDUR_EVENT);
 if (!h) return;
 memset(h, 0, sizeof(gles1::Header) + (count - 1) * sizeof(gles1::MeshInstance));
 h->platform = PLATFORM_GLES1;
 h->serial = g->meshHeader->serialNum;
 h->count = count;
 g->instData = h;

 gles1::Im3DVertex *v = (gles1::Im3DVertex*)rwMalloc(g->numVertices * sizeof(gles1::Im3DVertex), MEMDUR_FUNCTION);
 if (!v) { gles1::freeInstanceData(g); return; }

 Mesh *mesh = g->meshHeader->getMeshes();
 for (unsigned j = 0; j < count; j++) {
  RGBA mat = mesh[j].material->color;
  for (int i = 0; i < g->numVertices; i++) {
   memset(&v[i], 0, sizeof(v[i]));
   v[i].position = g->morphTargets[0].vertices[i];
   if (g->morphTargets[0].normals) v[i].normal = g->morphTargets[0].normals[i];
   RGBA c = g->colors ? g->colors[i] : makeRGBA(255, 255, 255, 255);
   v[i].setColor(c.red * mat.red / 255, c.green * mat.green / 255, c.blue * mat.blue / 255, c.alpha * mat.alpha / 255);
   if (v[i].a < 255) h->meshes[j].alpha = true;
   if (g->texCoords[0]) { v[i].u = g->texCoords[0][i].u; v[i].v = g->texCoords[0][i].v; }
  }
  uint32_t numIndices = mesh[j].numIndices;
  h->meshes[j].vertices = (gles1::Im3DVertex*)rwMalloc(numIndices * sizeof(gles1::Im3DVertex), MEMDUR_EVENT);
  if (!h->meshes[j].vertices) { rwFree(v); gles1::freeInstanceData(g); return; }
  for (uint32_t k = 0; k < numIndices; k++) {
   h->meshes[j].vertices[k] = v[mesh[j].indices[k]];
  }
 }
 glBindBuffer(GL_ARRAY_BUFFER, 0);
 rwFree(v);
 g->lockedSinceInst = 0;
}

static void m8Uninstance(ObjPipeline*, Atomic *a) {
 gles1::freeInstanceData(a->geometry);
}

static void m8Render(ObjPipeline *p, Atomic *a) {
 m8Instance(p, a);
 Geometry *g = a->geometry;
 if (!g->instData) return;
 gles1::Header *h = (gles1::Header*)g->instData;
 Mesh *meshes = g->meshHeader->getMeshes();
 gles1::restore3D();
 gles1::loadWorld(a->getFrame()->getLTM());
 uintptr savedTex = (uintptr)GetRenderStatePtr(TEXTURERASTER);
 uint32 savedAlpha = GetRenderState(VERTEXALPHA);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
 for (unsigned j = 0; j < h->count; j++) {
  Mesh *mesh = &meshes[j];
  Texture *t = mesh->material->texture;
  Raster *r = t ? t->raster : nil;
  SetRenderStatePtr(TEXTURERASTER, r);
  SetRenderState(VERTEXALPHA, h->meshes[j].alpha || (r && r->format == Raster::C4444));
  if (t) {
   SetRenderState(TEXTUREFILTER, t->getFilter());
   SetRenderState(TEXTUREADDRESSU, t->getAddressU());
   SetRenderState(TEXTUREADDRESSV, t->getAddressV());
  }
  gles1::applyStates();
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  gles1::Im3DVertex *v = h->meshes[j].vertices;
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(3, GL_FLOAT, sizeof(gles1::Im3DVertex), v);
  glEnableClientState(GL_COLOR_ARRAY);
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(gles1::Im3DVertex), (uint8*)v + offsetof(gles1::Im3DVertex, r));
  glDisableClientState(GL_NORMAL_ARRAY);
  if (r) {
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, sizeof(gles1::Im3DVertex), (uint8*)v + offsetof(gles1::Im3DVertex, u));
  } else {
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  glDrawArrays(GL_TRIANGLES, 0, mesh->numIndices);
 }
 glBindBuffer(GL_ARRAY_BUFFER, 0);
 SetRenderStatePtr(TEXTURERASTER, (void*)savedTex);
 SetRenderState(VERTEXALPHA, savedAlpha);
}

static ObjPipeline *m8Pipe = nil;
static ObjPipeline *getM8Pipeline() {
 if (!m8Pipe) {
  m8Pipe = ObjPipeline::create();
  m8Pipe->platform = PLATFORM_GLES1;
  m8Pipe->impl.instance = m8Instance;
  m8Pipe->impl.uninstance = m8Uninstance;
  m8Pipe->impl.render = m8Render;
 }
 return m8Pipe;
}

struct ModelSlot {
 Texture *textures[64];
 Frame *frame;
 RpClump *clump;
 RwTexDictionary *dictionary;
};
static ModelSlot slots[2];
static ModelSlot *current;

void unloadBlock(unsigned slot){
 current=&slots[slot];
 if(current->clump){RpClumpDestroy(current->clump);current->clump=nil;current->frame=nil;}
 if(current->frame){current->frame->destroy();current->frame=nil;}
 if(current->dictionary){RwTexDictionaryDestroy(current->dictionary);current->dictionary=nil;}
 memset(current->textures,0,sizeof(current->textures));
}

bool loadBlock(unsigned slot,const NanPack &pack){
 current=&slots[slot];
 const NanTexture *modelTextures=pack.textures;
 const NanPart *modelParts=pack.parts;
 if(current->frame || pack.numTextures>64)return false;
 RwTexDictionary *previousDictionary=RwTexDictionaryGetCurrent();
 current->dictionary=RwTexDictionaryCreate();if(!current->dictionary)return false;
 RwTexDictionarySetCurrent(current->dictionary);
 current->clump=RpClumpCreate();if(!current->clump){RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
 current->frame=Frame::create();if(!current->frame){RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}RpClumpSetFrame(current->clump,current->frame);
 for(unsigned i=0;i<pack.numTextures;i++){
  const NanTexture &t=modelTextures[i];Raster *r=Raster::create(t.width,t.height,16,Raster::TEXTURE|(t.format==1?Raster::C565:Raster::C4444));
  if(!r){RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
  uint8 *p=r->lock(0,Raster::LOCKWRITE);if(!p){r->destroy();RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
  memcpy(p,t.pixels,t.width*t.height*2);r->unlock(0);current->textures[i]=RwTextureCreate(r);if(!current->textures[i]){r->destroy();RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}RwTextureSetName(current->textures[i],t.name);RwTexDictionaryAddTexture(current->dictionary,current->textures[i]);current->textures[i]->setFilter(Texture::NEAREST);
 }
 for(unsigned i=0;i<pack.numParts;i++){
  const NanPart &p=modelParts[i];Geometry *g=Geometry::create(p.vertices,p.triangles,Geometry::POSITIONS|Geometry::TEXTURED|Geometry::PRELIT);
  if(!g){RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
  for(int j=0;j<p.vertices;j++){
   const NanVertex &v=p.data[j];g->morphTargets[0].vertices[j]={v.x,v.y,v.z};g->texCoords[0][j]={v.u,v.v};g->colors[j]=makeRGBA(255,255,255,v.a);
  }
  for(int j=0;j<p.triangles;j++){g->triangles[j].v[0]=p.indices[j*3];g->triangles[j].v[1]=p.indices[j*3+1];g->triangles[j].v[2]=p.indices[j*3+2];g->triangles[j].matId=0;}
  Material *m=Material::create();if(!m){g->destroy();RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}if(p.texture>=0)m->setTexture(RwTexDictionaryFindNamedTexture(current->dictionary,modelTextures[p.texture].name));g->matList.appendMaterial(m);m->destroy();g->calculateBoundingSphere();g->buildMeshes();
  Atomic *atomic=Atomic::create();if(!atomic){g->destroy();RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
  atomic->pipeline = getM8Pipeline();
  RpClumpAddAtomic(current->clump,atomic);atomic->setFrame(current->frame);atomic->setGeometry(g,0);g->destroy();
 }
 if(!GetFirstAtomic(current->clump)||!GetFirstTexture(current->dictionary)){RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
 RwTexDictionarySetCurrent(previousDictionary);return true;
}

void renderBlock(unsigned slot){
 current=&slots[slot];
 RpClumpRender(current->clump);
}
