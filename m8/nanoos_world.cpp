#include "model.h"
#include <stdint.h>
extern "C" {
#include "asset.h"
}
// Mode 3: the 112-triangle preview (preview.bin, tools/make_world_preview.py),
// one triangle per part. Mode 4: the Vice City street block (vcworld.bin,
// tools/make_vc_world.py), drawn as whole 8 m chunks (nanoos_model.inc renderVc); only
// its textures go through block 0. Both come from the vcworld.pak runtime
// pack (asset_find, 0x08400000): the kernel is at its 64 KiB cap.
#define MAX_TRIS 256
static NanVertex vertices[MAX_TRIS*3];
static uint16_t indices[MAX_TRIS*3];
static NanPart parts[MAX_TRIS];
static NanTexture textures[32];
static NanPack pack;
// Triangle i from s16 xyz (1/16 unit) and uv (1/256), white.
static void setTri(unsigned i,const int16_t *xyz,const int16_t *uv,int tex){
 for(unsigned j=0;j<3;j++)vertices[i*3+j]={xyz[j*3]/16.f,xyz[j*3+1]/16.f,xyz[j*3+2]/16.f,uv[j*2]/256.f,uv[j*2+1]/256.f,255,255,255,255};
 parts[i]={3,1,tex,vertices+i*3,indices+i*3};
}
// "NVP1": u16 textures, triangles; u16 texels[textures][64]; triangles x
// { s16 xyz[9], uv[6]; u16 texture }
struct PreviewTriangle { int16_t xyz[9],uv[6]; uint16_t texture; };
static unsigned buildPreview(){
 uint32_t size=0;
 const uint8_t *p=(const uint8_t*)asset_find("preview.bin",&size);
 if(!p||size<8||p[0]!='N'||p[1]!='V'||p[2]!='P'||p[3]!='1')return 0;
 unsigned nt=p[4]|p[5]<<8,count=p[6]|p[7]<<8;
 if(!nt||nt>31||!count||count>MAX_TRIS||size<8+nt*128+count*sizeof(PreviewTriangle))return 0;
 for(unsigned i=0;i<nt;i++)textures[i]={8,8,1,p+8+i*128,"preview",128,0};
 const PreviewTriangle *t=(const PreviewTriangle*)(p+8+nt*128);
 for(unsigned i=0;i<count;i++){if(t[i].texture>=nt)return 0;setTri(i,t[i].xyz,t[i].uv,t[i].texture);}
 pack.numParts=count;return nt;
}
// "NVW3" (tools/make_vc_world.py): u16 textures, models, grounds,
// triangles; textures {u16 w,h; rgb565[w*h]}; VcModel[]; VcGround[];
// VcTri[]. Models, grounds and triangles are used in place in the pack.
static unsigned buildVc(){
 uint32_t size=0;
 const uint8_t *p=(const uint8_t*)asset_find("vcworld.bin",&size);
 if(!p||size<12||p[0]!='N'||p[1]!='V'||p[2]!='W'||p[3]!='3')return 0;
 const uint8_t *end=p+size;
 unsigned nt=p[4]|p[5]<<8,nm=p[6]|p[7]<<8,ng=p[8]|p[9]<<8,count=p[10]|p[11]<<8;
 if(!nt||nt>31||!nm||!count)return 0;
 p+=12;
 for(unsigned i=0;i<nt;i++){
  if(end-p<4)return 0;
  unsigned w=p[0]|p[1]<<8,h=p[2]|p[3]<<8,bytes=w*h*2;
  if((unsigned)(end-p-4)<bytes)return 0;
  textures[i]={(int)w,(int)h,1,p+4,"vc",bytes,0};p+=4+bytes;
 }
 if((unsigned)(end-p)<nm*sizeof(VcModel)+ng*sizeof(VcGround)+count*sizeof(VcTri))return 0;
 const VcModel *m=(const VcModel*)p;const VcGround *g=(const VcGround*)(m+nm);
 const VcTri *t=(const VcTri*)(g+ng);
 for(unsigned i=0;i<nm;i++)if(m[i].first0+m[i].count0>count||m[i].first1+m[i].count1>count)return 0;
 for(unsigned i=0;i<count;i++)if(t[i].tex>=nt)return 0;
 m8Vc={m,g,t,nm,ng};pack.numParts=0;return nt;
}
bool m8_world_init(bool vc){
 for(unsigned i=0;i<MAX_TRIS*3;i++)indices[i]=i%3;
 m8Vc={0,0,0,0,0};
 unsigned nt=vc?buildVc():buildPreview();
 if(!nt){m8Vc={0,0,0,0,0};return false;}
 pack.textures=textures;pack.parts=parts;pack.numTextures=nt;
 return loadBlock(0,pack)&&m8PrepareBatches();
}
void m8_world_shutdown(){unloadBlock(0);}
int m8_world_draw(const M8Cam &cam){return m8NanoModelRender(cam)?0:-1;}
