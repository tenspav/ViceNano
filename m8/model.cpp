#ifdef M8_NANOOS
#include "nanoos_model.inc"
#else
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
#include "../vendor/librw/src/gles1/rwgles1.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

extern "C" {
#include "gl.h"
}

#include "model.h"

using namespace rw;

M8RenderStats m8Stats;
extern "C" void glCompressedTexImage2D(GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const GLvoid*);
static Raster *uploadTexture(const NanTexture &t){
 if(t.width<1 || t.height<1 || t.bytes!=512u+unsigned(t.width*t.height) ||
    t.compressedFormat!=(t.format==1?0x8b97u:0x8b98u))return nil;
 Raster *r=Raster::create(t.width,t.height,16,Raster::TEXTURE|Raster::DONTALLOCATE|
                        (t.format==1?Raster::C565:Raster::C4444));
 if(!r)return nil;
 gles1::NativeRaster *n=gles1::nativeRaster(r);
 glGenTextures(1,&n->texture);glBindTexture(GL_TEXTURE_2D,n->texture);
 glCompressedTexImage2D(GL_TEXTURE_2D,0,t.compressedFormat,t.width,t.height,0,t.bytes,t.pixels);
 GLenum error=glGetError();
#ifdef M8_FORCE_TEXTURE_FALLBACK
 error=0x0502; // Exercise the same retry as a driver rejecting compressed upload.
#endif
 if(error!=GL_NO_ERROR){
  glDeleteTextures(1,&n->texture);n->texture=0;
  uint16_t *raw=(uint16_t*)rwMalloc(t.width*t.height*2,MEMDUR_FUNCTION);
  if(!raw){r->destroy();return nil;}
  const uint8_t *indices=t.pixels+512;
  for(int i=0;i<t.width*t.height;i++){
   unsigned p=indices[i]*2;raw[i]=t.pixels[p]|(t.pixels[p+1]<<8);
  }
  glGenTextures(1,&n->texture);glBindTexture(GL_TEXTURE_2D,n->texture);
  GLenum fmt=t.format==1?GL_RGB:GL_RGBA;glPixelStorei(GL_UNPACK_ALIGNMENT,2);
  glTexImage2D(GL_TEXTURE_2D,0,fmt,t.width,t.height,0,fmt,
               t.format==1?GL_UNSIGNED_SHORT_5_6_5:GL_UNSIGNED_SHORT_4_4_4_4,raw);
  rwFree(raw);m8Stats.textureFallbacks++;
  if(glGetError()!=GL_NO_ERROR){r->destroy();return nil;}
 }
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 n->bytes=t.width*t.height*2; // This driver expands PAL8 to 16-bit texels.
 m8Stats.textureUploads++;return r;
}

// The world pack already contains transformed, static positions. Build shared
// material batches once when residency changes, with an index stream for each
// visibility mask. Keep ushort indices local to a batch, not the entire city.

struct TextureEntry { const NanTexture *source; Raster *raster; unsigned refs; };
static TextureEntry textures[128];
struct ModelSlot { const NanPack *pack; Sphere sphere; unsigned textures[64]; };
static ModelSlot slots[2];
struct Batch {
 GLuint texture;
 bool alpha;
 Sphere spheres[2];
 uint32_t firstVertex, vertices, firstIndex[4], indices[4], baseVertex[4], visibleVertices[4];
 // De-indexed triangle list: slot 0 triangles then slot 1, so every mask is one range.
 uint32_t flatFirst[4], flatCount[4];
};
struct PartRef { const NanPart *part; unsigned slot,batch; uint32_t baseVertex; };
static Batch batches[128];
static unsigned numBatches;
static M8Vertex *vertices;
static uint16_t *indices;
static M8Vertex *flat;
static bool dirty=true;
// Indexed by default: glDrawElements costs the same per call as glDrawArrays
// (P0 bench) and de-indexing ~doubles the vertices sent. -DM8_ARRAY_DRAWS
// selects the de-indexed glDrawArrays path (flat buffer) for device A/B runs.
#ifdef M8_ARRAY_DRAWS
bool m8IndexedDraws=false;
#else
bool m8IndexedDraws=true;
#endif

static void freeBatches(){
 if(vertices)rwFree(vertices);
 if(indices)rwFree(indices);
 if(flat)rwFree(flat);
 vertices=nil;indices=nil;flat=nil;numBatches=0;dirty=true;
}

static int acquireTexture(const NanTexture &t){
 int vacant=-1;
 for(unsigned i=0;i<128;i++){
  TextureEntry &entry=textures[i];
  if(!entry.refs){if(vacant<0)vacant=i;continue;}
  const NanTexture &s=*entry.source;
  if(s.width==t.width && s.height==t.height && s.format==t.format &&
     s.compressedFormat==t.compressedFormat && s.bytes==t.bytes &&
     !memcmp(s.pixels,t.pixels,t.bytes)){entry.refs++;return i;}
 }
 if(vacant<0)return -1;
 Raster *r=uploadTexture(t);if(!r)return -1;
 textures[vacant]={&t,r,1};
 m8Stats.texturePackedBytes+=t.bytes;
 m8Stats.textureResidentBytes+=t.width*t.height*2;
 return vacant;
}

void unloadBlock(unsigned slot){
 if(slot>=2 || !slots[slot].pack)return;
 freeBatches();
 ModelSlot &s=slots[slot];
 for(unsigned i=0;i<s.pack->numTextures;i++){
  TextureEntry &entry=textures[s.textures[i]];
  if(--entry.refs==0){
   m8Stats.texturePackedBytes-=entry.source->bytes;
   m8Stats.textureResidentBytes-=entry.source->width*entry.source->height*2;
   entry.raster->destroy();entry={};
  }
 }
 s={};
}

bool loadBlock(unsigned slot,const NanPack &pack){
 if(slot>=2 || slots[slot].pack || pack.numTextures>64 || pack.numParts>64)return false;
 ModelSlot &s=slots[slot];
 unsigned loaded=0;
 for(;loaded<pack.numTextures;loaded++){
  int texture=acquireTexture(pack.textures[loaded]);
  if(texture<0)break;
  s.textures[loaded]=texture;
 }
 if(loaded!=pack.numTextures){
  // Use the normal release path for the successfully acquired prefix.
  NanPack partial=pack;partial.numTextures=loaded;s.pack=&partial;unloadBlock(slot);return false;
 }
 s.pack=&pack;
 float minX=1e30f,maxX=-1e30f,minY=1e30f,maxY=-1e30f,minZ=1e30f,maxZ=-1e30f;
 for(unsigned i=0;i<pack.numParts;i++){
  const NanPart &p=pack.parts[i];
  if(p.vertices<1 || p.triangles<0 || p.texture<-1 || p.texture>=(int)pack.numTextures){unloadBlock(slot);return false;}
  for(int j=0;j<p.vertices;j++){
   const NanVertex &v=p.data[j];
   if(v.x<minX)minX=v.x;if(v.x>maxX)maxX=v.x;
   if(v.y<minY)minY=v.y;if(v.y>maxY)maxY=v.y;
   if(v.z<minZ)minZ=v.z;if(v.z>maxZ)maxZ=v.z;
  }
  for(int j=0;j<p.triangles*3;j++)if(p.indices[j]>=p.vertices){unloadBlock(slot);return false;}
 }
 s.sphere.center={(minX+maxX)*0.5f,(minY+maxY)*0.5f,(minZ+maxZ)*0.5f};
 float radius2=0;
 for(unsigned i=0;i<pack.numParts;i++){
  const NanPart &p=pack.parts[i];
  for(int j=0;j<p.vertices;j++){
   const NanVertex &v=p.data[j];
   float dx=v.x-s.sphere.center.x,dy=v.y-s.sphere.center.y,dz=v.z-s.sphere.center.z;
   float d=dx*dx+dy*dy+dz*dz;if(d>radius2)radius2=d;
  }
 }
 s.sphere.radius=sqrtf(radius2)*1.02f;
 dirty=true;return true;
}

bool m8PrepareBatches(){
 if(!dirty)return true;
 freeBatches();
 PartRef refs[128];unsigned numRefs=0,barrier=0;
 uint32_t totalVertices=0,totalIndices=0;
 for(unsigned slot=0;slot<2;slot++){
  const NanPack *pack=slots[slot].pack;if(!pack)continue;
  for(unsigned i=0;i<pack->numParts;i++){
   const NanPart &p=pack->parts[i];if(!p.triangles)continue;
   GLuint texture=0;bool alpha=false;
   if(p.texture>=0){
    TextureEntry &entry=textures[slots[slot].textures[p.texture]];
    texture=gles1::nativeRaster(entry.raster)->texture;
    alpha=pack->textures[p.texture].format==2;
   }
   for(int v=0;v<p.vertices;v++)if(p.data[v].a<255)alpha=true;
   unsigned b=numBatches;
   // Do not move any opaque triangles across a translucent draw, nor reorder
   // translucent parts. Vertex/material alpha is part of the batching key.
   if(!alpha)for(unsigned j=barrier;j<numBatches;j++){
    if(!batches[j].alpha && batches[j].texture==texture &&
       batches[j].vertices+p.vertices<=65535){b=j;break;}
   }
   if(b==numBatches){batches[b]={};batches[b].texture=texture;batches[b].alpha=alpha;numBatches++;}
   if(p.vertices>65535 || numRefs>=128){freeBatches();return false;}
   refs[numRefs++]={&p,slot,b,batches[b].vertices};
   batches[b].vertices+=p.vertices;
   if(alpha)barrier=numBatches;
   totalVertices+=p.vertices;totalIndices+=p.triangles*3;
  }
 }
 if(!numRefs){dirty=false;return true;}
 vertices=(M8Vertex*)rwMalloc(totalVertices*sizeof(M8Vertex),MEMDUR_EVENT);
 // Each slot's triangles occur in its individual mask and the combined mask.
 indices=(uint16_t*)rwMalloc(totalIndices*2*sizeof(uint16_t),MEMDUR_EVENT);
 if(!m8IndexedDraws)flat=(M8Vertex*)rwMalloc(totalIndices*sizeof(M8Vertex),MEMDUR_EVENT);
 if(!vertices || !indices || (!m8IndexedDraws && !flat)){freeBatches();return false;}
 uint32_t vertexCursor=0,indexCursor=0,flatCursor=0;
 for(unsigned b=0;b<numBatches;b++){
  Batch &batch=batches[b];batch.firstVertex=vertexCursor;
  for(unsigned i=0;i<numRefs;i++)if(refs[i].batch==b){
   const PartRef &ref=refs[i];const NanPart &p=*ref.part;
   for(int j=0;j<p.vertices;j++){
    const NanVertex &v=p.data[j];
    // Match M8's existing unlit white RGB material; retain vertex alpha.
    vertices[vertexCursor++]={v.x,v.y,v.z,255,255,255,v.a,v.u,v.v};
   }
  }
  for(unsigned mask=1;mask<4;mask++){
   batch.firstIndex[mask]=indexCursor;
   uint32_t base=65535;
   for(unsigned i=0;i<numRefs;i++)if(refs[i].batch==b && (mask&(1u<<refs[i].slot))){
    if(refs[i].baseVertex<base)base=refs[i].baseVertex;
   }
   batch.baseVertex[mask]=base==65535?0:base;
   for(unsigned i=0;i<numRefs;i++)if(refs[i].batch==b && (mask&(1u<<refs[i].slot))){
    const PartRef &ref=refs[i];const NanPart &p=*ref.part;
    for(int j=0;j<p.triangles*3;j++)indices[indexCursor++]=ref.baseVertex+p.indices[j]-base;
    batch.visibleVertices[mask]+=p.vertices;
   }
   batch.indices[mask]=indexCursor-batch.firstIndex[mask];
  }
  // Refs are slot-ordered, so this matches the combined mask's indexed order.
  uint32_t flatStart=flatCursor;
  if(flat)for(unsigned i=0;i<numRefs;i++)if(refs[i].batch==b){
   const PartRef &ref=refs[i];const NanPart &p=*ref.part;
   const M8Vertex *src=vertices+batch.firstVertex+ref.baseVertex;
   for(int j=0;j<p.triangles*3;j++)flat[flatCursor++]=src[p.indices[j]];
  }
  batch.flatCount[1]=batch.indices[1];batch.flatCount[2]=batch.indices[2];
  batch.flatCount[3]=batch.indices[1]+batch.indices[2];
  batch.flatFirst[1]=batch.flatFirst[3]=flatStart;batch.flatFirst[2]=flatStart+batch.indices[1];
  for(unsigned slot=0;slot<2;slot++){
   if(!batch.indices[1u<<slot])continue;
   V3d lo={1e30f,1e30f,1e30f},hi={-1e30f,-1e30f,-1e30f};
   for(unsigned i=0;i<numRefs;i++)if(refs[i].batch==b && refs[i].slot==slot){
    const M8Vertex *v=vertices+batch.firstVertex+refs[i].baseVertex;
    for(int j=0;j<refs[i].part->vertices;j++){
     if(v[j].x<lo.x)lo.x=v[j].x;if(v[j].x>hi.x)hi.x=v[j].x;
     if(v[j].y<lo.y)lo.y=v[j].y;if(v[j].y>hi.y)hi.y=v[j].y;
     if(v[j].z<lo.z)lo.z=v[j].z;if(v[j].z>hi.z)hi.z=v[j].z;
    }
   }
   Sphere &sphere=batch.spheres[slot];
   sphere.center={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};
   float radius2=0;
   for(unsigned i=0;i<numRefs;i++)if(refs[i].batch==b && refs[i].slot==slot){
    const M8Vertex *v=vertices+batch.firstVertex+refs[i].baseVertex;
    for(int j=0;j<refs[i].part->vertices;j++){
     float x=v[j].x-sphere.center.x,y=v[j].y-sphere.center.y,z=v[j].z-sphere.center.z;
     float d=x*x+y*y+z*z;if(d>radius2)radius2=d;
    }
   }
   sphere.radius=sqrtf(radius2)*1.05f;
  }
 }
 dirty=false;return true;
}

void m8ResetRenderState(){
 m8Stats.drawCalls=m8Stats.vertices=m8Stats.indices=0;
 glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);
 glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glDisable(GL_LIGHTING);glDisable(0x0BC0 /* GL_ALPHA_TEST */);
 glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);
 glEnableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);
}

void renderWorld(){
 unsigned mask=0;
 for(unsigned i=0;i<2;i++)if(slots[i].pack &&
   (!engine->currentCamera || engine->currentCamera->frustumTestSphere(&slots[i].sphere)!=Camera::SPHEREOUTSIDE))mask|=1u<<i;
 if(!mask || !vertices || dirty)return;
 GLuint texture=~0u;int alpha=-1;
 const M8Vertex *lastV=nil;
 for(unsigned b=0;b<numBatches;b++){
  const Batch &batch=batches[b];
  unsigned visible=mask;
  if(engine->currentCamera)for(unsigned slot=0;slot<2;slot++){
   if((visible&(1u<<slot)) && batch.indices[1u<<slot] &&
      engine->currentCamera->frustumTestSphere(&batch.spheres[slot])==Camera::SPHEREOUTSIDE)visible&=~(1u<<slot);
  }
  if(!batch.indices[visible])continue;
  if(texture!=batch.texture){
   texture=batch.texture;
   if(texture){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);}
   else glDisable(GL_TEXTURE_2D);
  }
  if(alpha!=batch.alpha){
   alpha=batch.alpha;
   if(alpha){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
   else{glDisable(GL_BLEND);glDepthMask(GL_TRUE);}
  }
  const bool indexed=m8IndexedDraws || !flat;
  const M8Vertex *v=indexed?vertices+batch.firstVertex+batch.baseVertex[visible]:flat;
  if(v!=lastV){
   glVertexPointer(3,GL_FLOAT,sizeof(M8Vertex),&v->x);
   glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(M8Vertex),&v->r);
   glTexCoordPointer(2,GL_FLOAT,sizeof(M8Vertex),&v->u);
   lastV=v;
  }
  if(indexed){
   glDrawElements(GL_TRIANGLES,batch.indices[visible],GL_UNSIGNED_SHORT,indices+batch.firstIndex[visible]);
   m8Stats.vertices+=batch.visibleVertices[visible];
  }else{
   glDrawArrays(GL_TRIANGLES,batch.flatFirst[visible],batch.flatCount[visible]);
   m8Stats.vertices+=batch.flatCount[visible];
  }
  m8Stats.drawCalls++;m8Stats.indices+=batch.indices[visible];
 }
}

#endif // M8_NANOOS
