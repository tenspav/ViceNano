#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
extern RpAtomic *GetFirstAtomic(RpClump*);
extern RwTexture *GetFirstTexture(RwTexDictionary*);
#include "generated/asset.h"
#include <string.h>
using namespace rw;
static Texture *textures[sizeof(modelTextures)/sizeof(modelTextures[0])];
static Atomic *atomics[sizeof(modelParts)/sizeof(modelParts[0])];
static Frame *frame;
static RpClump *clump;
static RwTexDictionary *dictionary, *previousDictionary;
void unloadModel(){
 if(clump){RpClumpDestroy(clump);clump=nil;frame=nil;}
 for(auto &a:atomics)a=nil;
 if(frame){frame->destroy();frame=nil;}
 if(dictionary){RwTexDictionarySetCurrent(previousDictionary);RwTexDictionaryDestroy(dictionary);dictionary=nil;}
 for(auto &t:textures)t=nil;
}
bool loadModel(){
 if(frame)return false;
 previousDictionary=RwTexDictionaryGetCurrent();
 dictionary=RwTexDictionaryCreate();if(!dictionary)return false;
 RwTexDictionarySetCurrent(dictionary);
 clump=RpClumpCreate();if(!clump){unloadModel();return false;}
 frame=Frame::create();if(!frame){unloadModel();return false;}RpClumpSetFrame(clump,frame);
 for(unsigned i=0;i<sizeof(modelTextures)/sizeof(modelTextures[0]);i++){
  const NanTexture &t=modelTextures[i];Raster *r=Raster::create(t.width,t.height,16,Raster::TEXTURE|(t.format==1?Raster::C565:Raster::C4444));
  if(!r){unloadModel();return false;}
  uint8 *p=r->lock(0,Raster::LOCKWRITE);if(!p){r->destroy();unloadModel();return false;}
  memcpy(p,t.pixels,t.width*t.height*2);r->unlock(0);textures[i]=RwTextureCreate(r);if(!textures[i]){r->destroy();unloadModel();return false;}RwTextureSetName(textures[i],t.name);RwTexDictionaryAddTexture(dictionary,textures[i]);textures[i]->setFilter(Texture::NEAREST);
 }
 for(unsigned i=0;i<sizeof(modelParts)/sizeof(modelParts[0]);i++){
  const NanPart &p=modelParts[i];Geometry *g=Geometry::create(p.vertices,p.triangles,Geometry::POSITIONS|Geometry::TEXTURED|Geometry::PRELIT);
  if(!g){unloadModel();return false;}
  for(int j=0;j<p.vertices;j++){
   const NanVertex &v=p.data[j];g->morphTargets[0].vertices[j]={v.x,v.y,v.z};g->texCoords[0][j]={v.u,v.v};g->colors[j]=makeRGBA(255,255,255,v.a); // Unlit asset preview; preserve source prelight in the converted pack.
  }
  for(int j=0;j<p.triangles;j++){g->triangles[j].v[0]=p.indices[j*3];g->triangles[j].v[1]=p.indices[j*3+1];g->triangles[j].v[2]=p.indices[j*3+2];g->triangles[j].matId=0;}
  Material *m=Material::create();if(!m){g->destroy();unloadModel();return false;}if(p.texture>=0)m->setTexture(RwTexDictionaryFindNamedTexture(dictionary,modelTextures[p.texture].name));g->matList.appendMaterial(m);m->destroy();g->calculateBoundingSphere();g->buildMeshes();
  atomics[i]=Atomic::create();if(!atomics[i]){g->destroy();unloadModel();return false;}RpClumpAddAtomic(clump,atomics[i]);atomics[i]->setFrame(frame);atomics[i]->setGeometry(g,0);g->destroy();
 }
 if(!GetFirstAtomic(clump)||!GetFirstTexture(dictionary)){unloadModel();return false;}
 return true;
}
void renderModel(unsigned angle){
 V3d axis={0,1,0},tilt={1,0,0};frame->rotate(&tilt,15,COMBINEREPLACE);frame->rotate(&axis,angle%360,COMBINEPOSTCONCAT);
 RpClumpRender(clump);
}
