#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
extern RpAtomic *GetFirstAtomic(RpClump*);
extern RwTexture *GetFirstTexture(RwTexDictionary*);
#include "model.h"
#include <string.h>
using namespace rw;
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
   const NanVertex &v=p.data[j];g->morphTargets[0].vertices[j]={v.x,v.y,v.z};g->texCoords[0][j]={v.u,v.v};g->colors[j]=makeRGBA(255,255,255,v.a); // Unlit asset preview; preserve source prelight in the converted pack.
  }
  for(int j=0;j<p.triangles;j++){g->triangles[j].v[0]=p.indices[j*3];g->triangles[j].v[1]=p.indices[j*3+1];g->triangles[j].v[2]=p.indices[j*3+2];g->triangles[j].matId=0;}
  Material *m=Material::create();if(!m){g->destroy();RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}if(p.texture>=0)m->setTexture(RwTexDictionaryFindNamedTexture(current->dictionary,modelTextures[p.texture].name));g->matList.appendMaterial(m);m->destroy();g->calculateBoundingSphere();g->buildMeshes();
  Atomic *atomic=Atomic::create();if(!atomic){g->destroy();RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}RpClumpAddAtomic(current->clump,atomic);atomic->setFrame(current->frame);atomic->setGeometry(g,0);g->destroy();
 }
 if(!GetFirstAtomic(current->clump)||!GetFirstTexture(current->dictionary)){RwTexDictionarySetCurrent(previousDictionary);unloadBlock(slot);return false;}
 RwTexDictionarySetCurrent(previousDictionary);return true;
}
void renderBlock(unsigned slot){
 current=&slots[slot];
 RpClumpRender(current->clump);
}
