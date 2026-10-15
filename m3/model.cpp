#include "../vendor/librw/rw.h"
#include "generated/asset.h"
#include <string.h>
using namespace rw;
static Texture *textures[sizeof(modelTextures)/sizeof(modelTextures[0])];
static Atomic *atomics[sizeof(modelParts)/sizeof(modelParts[0])];
static Frame *frame;
void unloadModel(){
 for(auto &a:atomics)if(a){a->destroy();a=nil;}
 if(frame){frame->destroy();frame=nil;}
 for(auto &t:textures)if(t){t->destroy();t=nil;}
}
bool loadModel(){
 if(frame)return false;
 frame=Frame::create();if(!frame)return false;
 for(unsigned i=0;i<sizeof(modelTextures)/sizeof(modelTextures[0]);i++){
  const NanTexture &t=modelTextures[i];Raster *r=Raster::create(t.width,t.height,16,Raster::TEXTURE|(t.format==1?Raster::C565:Raster::C4444));
  if(!r){unloadModel();return false;}
  uint8 *p=r->lock(0,Raster::LOCKWRITE);if(!p){r->destroy();unloadModel();return false;}
  memcpy(p,t.pixels,t.width*t.height*2);r->unlock(0);textures[i]=Texture::create(r);strncpy(textures[i]->name,t.name,31);textures[i]->name[31]=0;textures[i]->setFilter(Texture::NEAREST);
 }
 for(unsigned i=0;i<sizeof(modelParts)/sizeof(modelParts[0]);i++){
  const NanPart &p=modelParts[i];Geometry *g=Geometry::create(p.vertices,p.triangles,Geometry::POSITIONS|Geometry::TEXTURED|Geometry::PRELIT);
  for(int j=0;j<p.vertices;j++){
   const NanVertex &v=p.data[j];g->morphTargets[0].vertices[j]={v.x,v.y,v.z};g->texCoords[0][j]={v.u,v.v};g->colors[j]=makeRGBA(255,255,255,v.a); // Unlit asset preview; preserve source prelight in the converted pack.
  }
  for(int j=0;j<p.triangles;j++){g->triangles[j].v[0]=p.indices[j*3];g->triangles[j].v[1]=p.indices[j*3+1];g->triangles[j].v[2]=p.indices[j*3+2];g->triangles[j].matId=0;}
  Material *m=Material::create();if(p.texture>=0)m->setTexture(textures[p.texture]);g->matList.appendMaterial(m);m->destroy();g->calculateBoundingSphere();g->buildMeshes();
  atomics[i]=Atomic::create();atomics[i]->setFrame(frame);atomics[i]->setGeometry(g,0);g->destroy();
 }
 return true;
}
void renderModel(unsigned angle){
 V3d axis={0,1,0},tilt={1,0,0};frame->rotate(&tilt,15,COMBINEREPLACE);frame->rotate(&axis,angle%360,COMBINEPOSTCONCAT);
 // Actual reVC fakerw RpAtomicRender wrapper is compiled from the reference source.
 extern rw::Atomic *RpAtomicRender(rw::Atomic*);
 for(auto a:atomics)RpAtomicRender(a);
}
