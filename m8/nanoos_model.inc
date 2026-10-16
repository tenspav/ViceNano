// NanoOS backend of model.cpp: original NanPack/NanPart/NanTexture contracts.
#include "model.h"
#include "nanoos_shim.h"
#include <string.h>
M8RenderStats m8Stats;
bool m8IndexedDraws=false;
static const NanPack *packs[2];
static int textures[2][32];
static uint16_t upload[32*32];
struct ClipVertex { float x,y,z,u,v; uint32_t color; };
struct RenderTriangle { M8GpuTexVertex v[3]; float depth; int texture; };
static RenderTriangle triangles[256];
void unloadBlock(unsigned slot){
 if(slot>=2)return;
 for(unsigned i=0;i<32;i++)if(textures[slot][i]){
  m8_gpu_texture_destroy(textures[slot][i]);textures[slot][i]=0;
  const NanTexture &t=packs[slot]->textures[i];
  m8Stats.texturePackedBytes-=t.bytes;m8Stats.textureResidentBytes-=t.width*t.height*2;
 }
 packs[slot]=0;
}
bool loadBlock(unsigned slot,const NanPack &pack){
 if(slot>=2||pack.numTextures>32||!pack.textures||!pack.parts)return false;
 unloadBlock(slot);packs[slot]=&pack;
 for(unsigned i=0;i<pack.numTextures;i++){
  const NanTexture &t=pack.textures[i];
  if(t.width<8||t.height<8||t.width>32||t.height>32||(t.format!=1&&t.format!=2)||!t.pixels){unloadBlock(slot);return false;}
  unsigned count=t.width*t.height;
  bool pal=t.compressedFormat==(t.format==1?0x8b97u:0x8b98u)&&t.bytes==512+count;
  if(!pal && (t.compressedFormat||t.bytes!=count*2)){unloadBlock(slot);return false;}
  for(unsigned j=0;j<count;j++){
   unsigned pos=pal?t.pixels[512+j]*2:j*2;
   unsigned p=t.pixels[pos]|(unsigned)t.pixels[pos+1]<<8;
   upload[j]=t.format==1?p:(p>>4)|(p<<12); // RGBA4444 -> ARGB4444
  }
  int id=m8_gpu_texture_create(t.width,t.height,t.format,upload);
  if(id<0){unloadBlock(slot);return false;}
  textures[slot][i]=id;
  m8Stats.texturePackedBytes+=t.bytes;m8Stats.textureResidentBytes+=count*2;++m8Stats.textureUploads;
 }
 packs[slot]=&pack;return true;
}
bool m8PrepareBatches(){
 unsigned total=0;
 for(unsigned slot=0;slot<2;slot++)if(packs[slot]){
  const NanPack &pack=*packs[slot];
  for(unsigned i=0;i<pack.numParts;i++){
   const NanPart &p=pack.parts[i];
   if(p.vertices<=0||p.triangles<0||p.texture<0||(unsigned)p.texture>=pack.numTextures||!p.data||!p.indices)return false;
   total+=p.triangles;
   if(total>128)return false;
   for(int j=0;j<p.triangles*3;j++)if(p.indices[j]>=p.vertices)return false;
  }
 }
 return true;
}
void m8ResetRenderState(){m8Stats.drawCalls=m8Stats.vertices=m8Stats.indices=0;}
static uint32_t mixColor(uint32_t a,uint32_t b,float t){
 uint32_t out=0;
 for(unsigned i=0;i<32;i+=8){int x=(a>>i)&255,y=(b>>i)&255;out|=unsigned(x+int((y-x)*t))<<i;}
 return out;
}
static unsigned nearClip(const ClipVertex in[3],ClipVertex out[4]){
 unsigned n=0;
 for(unsigned i=0;i<3;i++){
  const ClipVertex &a=in[i],&b=in[(i+1)%3];
  bool ai=a.z>=8,bi=b.z>=8;
  if(ai)out[n++]=a;
  if(ai!=bi){float t=(8-a.z)/(b.z-a.z);out[n++]={a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),8,a.u+t*(b.u-a.u),a.v+t*(b.v-a.v),mixColor(a.color,b.color,t)};}
 }
 return n;
}
bool m8NanoModelRender(int pan){
 unsigned count=0;m8ResetRenderState();
 for(unsigned slot=0;slot<2;slot++)if(packs[slot]){
  const NanPack &pack=*packs[slot];
  for(unsigned part=0;part<pack.numParts;part++){
   const NanPart &p=pack.parts[part];
   for(int tri=0;tri<p.triangles;tri++){
    ClipVertex in[3],out[4];
    for(unsigned j=0;j<3;j++){
     const NanVertex &v=p.data[p.indices[tri*3+j]];
     in[j]={(v.x-v.z)*.70710678f+pan,(v.y-12)*.97f+(v.x+v.z)*.17f,(v.x+v.z)*.686f-(v.y-12)*.24f+175,v.u,v.v,(uint32_t(v.a)<<24)|(uint32_t(v.r)<<16)|(uint32_t(v.g)<<8)|v.b};
    }
    unsigned n=nearClip(in,out);
    for(unsigned i=1;i+1<n;i++){
     if(count==256)return false;
     const ClipVertex v[3]={out[0],out[i],out[i+1]};
     RenderTriangle r={};r.texture=textures[slot][p.texture];
     for(unsigned j=0;j<3;j++){r.v[j]={{216+v[j].x*210/v[j].z,120-v[j].y*210/v[j].z,0,v[j].color},v[j].u,v[j].v};r.depth+=v[j].z;}
     unsigned at=count++;
     while(at && triangles[at-1].depth<r.depth){triangles[at]=triangles[at-1];--at;}
     triangles[at]=r;
    }
   }
  }
 }
 for(unsigned i=0;i<count;i++){
  if(m8_gpu_textured_triangle(triangles[i].texture,triangles[i].v)<0)return false;
  ++m8Stats.drawCalls;m8Stats.vertices+=3;m8Stats.indices+=3;
 }
 return true;
}
void renderWorld(){(void)m8NanoModelRender(0);}
