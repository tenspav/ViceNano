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
struct RenderTriangle { M8GpuTexVertex v[3]; int texture; };
static RenderTriangle triangles[320];
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
   if(total>256)return false;
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
// View-space planes kept (a*x+b*y+c*z+e >= 0): the near plane (e = -near),
// then a guard band one panel wide/high around the 432x240 projection
// (210/z), so a close wall or road triangle reaches the TA with bounded
// screen coordinates.
static const float planes[5][3]={{0,0,1},{-210,0,432},{210,0,432},{0,-210,240},{0,210,240}};
static unsigned clipPlane(const ClipVertex *in,unsigned n,ClipVertex *out,const float *k,float e){
 unsigned m=0;float d[8];
 for(unsigned i=0;i<n;i++)d[i]=k[0]*in[i].x+k[1]*in[i].y+k[2]*in[i].z+e;
 for(unsigned i=0;i<n;i++){
  unsigned j=i+1<n?i+1:0;const ClipVertex &a=in[i],&b=in[j];
  if(d[i]>=0)out[m++]=a;
  if((d[i]>=0)!=(d[j]>=0)){
   float t=d[i]/(d[i]-d[j]);const float *fa=&a.x,*fb=&b.x;float *o=&out[m].x;
   for(unsigned f=0;f<5;f++)o[f]=fa[f]+t*(fb[f]-fa[f]); // x y z u v
   out[m++].color=mixColor(a.color,b.color,t);
  }
 }
 return m;
}
// Walk-mode street block (NVW3, nanoos_world.cpp): the models' faithful
// meshes (vc_simplify.py, within 0.1 m and 1 texel of the source) cut into
// 8 m chunks of whole triangles. Visible chunks are drawn whole, nearest
// first, until one would take the frame past VC_DRAW drawn triangles (those
// within VC_NEAR_M always go in, up to the 320 queue); the frame's fog then
// reaches the clear colour at that chunk's distance, so nothing beyond it is
// missed. Round 6 drew whole models at 300/110-triangle LODs: the device
// showed them warped (p95 1-3 m, max 7-12 m off the source, textures 30-77
// texels off). VC_DRAW 148: 149 segments stay under the driver's 150-segment
// re-arm rule (mbx.h MBX_REARM_BIG_SEGS), and 148 drawn ran at 6 fps.
M8VcScene m8Vc;
#define VC_DRAW 148u
#define VC_NEAR_M 16.f
#define VC_FAR_M 160.f
#define VC_CLEAR 0xff0d1440u
static unsigned count;
static uint32_t keys[320];    // depth << 9 | index, drawn far to near
static void sortKeys(uint32_t *k,unsigned n){ // descending, shell sort
 for(unsigned gap=n/2;gap;gap=gap==2?1:gap*5/11)for(unsigned i=gap;i<n;i++){
  uint32_t x=k[i];unsigned at=i;while(at>=gap&&k[at-gap]<x){k[at]=k[at-gap];at-=gap;}k[at]=x;
 }
}
static void view(const M8Cam &cam,float x,float y,float z,float *o){
 float dx=x-cam.x,dy=y-cam.y,dz=z-cam.z,d=dx*cam.s+dz*cam.c;
 o[0]=dx*cam.c-dz*cam.s;o[1]=dy*cam.pc+d*cam.ps;o[2]=d*cam.pc-dy*cam.ps+cam.back;
}
// Clip (only against the planes a vertex is outside of), project, cull,
// queue. in[] has room for 8 vertices.
static void addTri(ClipVertex *in,int texture,const M8Cam &cam){
 unsigned out3[3]={0,0,0},n=3;
 for(unsigned j=0;j<3;j++)for(unsigned k=0;k<5;k++)
  if(planes[k][0]*in[j].x+planes[k][1]*in[j].y+planes[k][2]*in[j].z<(k?0:cam.near))out3[j]|=1u<<k;
 if(out3[0]&out3[1]&out3[2])return;  // all outside one plane
 unsigned any=out3[0]|out3[1]|out3[2];
 for(unsigned k=0;k<5&&n;k++)if(any>>k&1){
  ClipVertex out[8];n=clipPlane(in,n,out,planes[k],k?0:-cam.near);
  for(unsigned i=0;i<n;i++)in[i]=out[i];
 }
 for(unsigned i=1;i+1<n;i++){
  if(count==320)return; // full: drop the rest rather than fail the frame
  const ClipVertex v[3]={in[0],in[i],in[i+1]};
  RenderTriangle &r=triangles[count];float depth=0;
  for(unsigned j=0;j<3;j++){r.v[j]={{216+v[j].x*210/v[j].z,120-v[j].y*210/v[j].z,v[j].z,v[j].color},v[j].u,v[j].v};depth+=v[j].z;}
  const M8GpuVertex &a=r.v[0].p,&b=r.v[1].p,&c=r.v[2].p;
  if(cam.cull&&(b.x-a.x)*(c.y-a.y)<(c.x-a.x)*(b.y-a.y))continue;
  r.texture=texture;
  uint32_t q=depth<0?0:depth>65535?65535*128:(uint32_t)(depth*128); // 1/384 m
  keys[count]=q<<9|count;++count;
 }
}
// Ground height of the grid square nearest (x, z); 'fallback' if none.
float m8VcGround(float x,float z,float fallback){
 float best=1e30f;
 for(unsigned i=0;i<m8Vc.numGrounds;i++){
  const VcGround &g=m8Vc.grounds[i];
  float dx=g.x/16.f-x,dz=g.z/16.f-z,d=dx*dx+dz*dz;
  if(d<best){best=d;fallback=g.h/16.f;}
 }
 return fallback;
}
static void addModel(const M8Cam &cam,unsigned first,unsigned n){
 for(const VcTri *t=m8Vc.tris+first,*e=t+n;t<e;t++){
  ClipVertex in[8];
  for(unsigned j=0;j<3;j++){
   float o[3];view(cam,t->xyz[j*3]/16.f,t->xyz[j*3+1]/16.f,t->xyz[j*3+2]/16.f,o);
   in[j]={o[0],o[1],o[2],t->uv[j*2]/256.f,t->uv[j*2+1]/256.f,0xff000000u|t->rgb[j*3]<<16|t->rgb[j*3+1]<<8|t->rgb[j*3+2]};
  }
  addTri(in,textures[0][t->tex],cam);
 }
}
// Returns the fog distance: where the first chunk left out starts.
static float renderVc(const M8Cam &cam){
 static uint32_t vk[512];unsigned nv=0; // ~(near distance << 9 | chunk): nearest first
 for(unsigned i=0;i<m8Vc.numModels&&i<512;i++){
  const VcModel &m=m8Vc.models[i];float o[3],r=m.r/16.f;
  view(cam,m.x/16.f,m.y/16.f,m.z/16.f,o);
  if(o[2]<-r||o[2]-r>VC_FAR_M||-210*o[0]+432*o[2]<-480*r||210*o[0]+432*o[2]<-480*r)continue;
  // round 8: conservative vertical sphere tests (|y|/z <= 240/210, twice the
  // 120-row half height; 319 >= |(240,210)|) and wholly before the near plane
  if(240*o[2]-210*o[1]<-319*r||240*o[2]+210*o[1]<-319*r||o[2]+r<cam.near)continue;
  float d=o[2]-r;vk[nv++]=~((d>0?(uint32_t)(d*64):0)<<9|i);
 }
 sortKeys(vk,nv);
 for(unsigned k=0;k<nv;k++){
  unsigned i=~vk[k]&511;const VcModel &m=m8Vc.models[i];
  unsigned start=count;addModel(cam,m.first0,m.count0);
  float d=(~vk[k]>>9)/64.f;
  if(count>=320||(count>VC_DRAW&&d>=VC_NEAR_M)){count=start;return d;}
 }
 return VC_FAR_M;
}
bool m8NanoModelRender(const M8Cam &cam){
 count=0;m8ResetRenderState();
 for(unsigned slot=0;slot<2;slot++)if(packs[slot]){
  const NanPack &pack=*packs[slot];
  for(unsigned part=0;part<pack.numParts;part++){
   const NanPart &p=pack.parts[part];
   for(int tri=0;tri<p.triangles;tri++){
    ClipVertex in[8];
    for(unsigned j=0;j<3;j++){
     const NanVertex &v=p.data[p.indices[tri*3+j]];float o[3];view(cam,v.x,v.y,v.z,o);
     in[j]={o[0],o[1],o[2],v.u,v.v,(uint32_t(v.a)<<24)|(uint32_t(v.r)<<16)|(uint32_t(v.g)<<8)|v.b};
    }
    addTri(in,textures[slot][p.texture],cam);
   }
  }
 }
 // fog: reaches the clear colour where the scene stops (eases out, cuts in)
 static float fog=VC_FAR_M;float end=VC_FAR_M*4;
 if(m8Vc.models&&cam.cull){float cut=renderVc(cam);fog=cut<fog?cut:fog+(cut-fog)*.25f;end=fog;}
 // far to near (painter's); 4-byte keys move, not the 80-byte triangles
 sortKeys(keys,count);
 for(unsigned i=0;i<count;i++){
  RenderTriangle &r=triangles[keys[i]&511];
  for(unsigned j=0;j<3;j++){
   float z=r.v[j].p.z;r.v[j].p.z=0;
   if(z>end*.6f)r.v[j].p.argb=mixColor(r.v[j].p.argb,VC_CLEAR,z>end?1:(z-end*.6f)/(end*.4f));
  }
  if(m8_gpu_textured_triangle(r.texture,r.v)<0)return false;
  ++m8Stats.drawCalls;m8Stats.vertices+=3;m8Stats.indices+=3;
 }
 return true;
}
void renderWorld(){static const M8Cam c={0,12,0,.70710678f,.70710678f,.97f,.24f,175,8,false};(void)m8NanoModelRender(c);}
