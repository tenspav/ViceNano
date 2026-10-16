#include "model.h"
#include <stdint.h>
#include "world_preview.h"
static NanVertex vertices[128*3];
static uint16_t indices[128*3];
static NanPart parts[128];
static NanTexture textures[sizeof(previewTexels)/sizeof(previewTexels[0])];
static NanPack pack;
bool m8_world_init(){
 for(unsigned i=0;i<sizeof(textures)/sizeof(textures[0]);i++)textures[i]={8,8,1,(const uint8_t*)previewTexels[i],"preview",128,0};
 const unsigned count=sizeof(preview)/sizeof(preview[0]);
 for(unsigned i=0;i<count;i++){
  const PreviewTriangle &t=preview[i];
  for(unsigned j=0;j<3;j++){
   vertices[i*3+j]={t.xyz[j*3]/16.f,t.xyz[j*3+1]/16.f,t.xyz[j*3+2]/16.f,t.uv[j*2]/256.f,t.uv[j*2+1]/256.f,255,255,255,255};
   indices[i*3+j]=j;
  }
  parts[i]={3,1,t.texture,vertices+i*3,indices+i*3};
 }
 pack={textures,parts,sizeof(textures)/sizeof(textures[0]),count,0,0,0,0,0,0};
 return loadBlock(0,pack)&&m8PrepareBatches();
}
void m8_world_shutdown(){unloadBlock(0);}
int m8_world_draw(int pan){return m8NanoModelRender(pan)?0:-1;}
