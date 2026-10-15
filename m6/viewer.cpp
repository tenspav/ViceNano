#include "nano.h"
#include "audio.h"
#include "stream.h"
#include <string.h>
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
extern "C" {
#include "hb_gl_surface.h"
#include "hb_heap.h"
#include "gl.h"
void nano_run_constructors();
}
using namespace rw;
static unsigned errors,unloadedHeap,loadedHeap,heapLow=~0u,baselineLow=~0u,baselineHigh,audits;
static unsigned stateLow[8],stateHigh[8],stateLive[8],maxStateRange;
static bool ready,balanced=true;
static gles1::Im2DVertex overlay[128*4];
static uint16 overlayIndices[128*6];
static unsigned quads;
static void tile(int x,int y,int w,int h,unsigned r,unsigned g,unsigned b){
 if(quads==128){im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,overlay,quads*4,overlayIndices,quads*6);quads=0;}
 gles1::Im2DVertex *v=overlay+quads*4;
 memset(v,0,sizeof(*v)*4);
 v[0].x=x;v[0].y=y;v[1].x=x+w;v[1].y=y;v[2].x=x+w;v[2].y=y+h;v[3].x=x;v[3].y=y+h;
 for(int i=0;i<4;i++){v[i].w=1;v[i].setColor(r,g,b,255);}
 const uint16 ix[]={0,1,2,0,2,3};for(unsigned i=0;i<6;i++)overlayIndices[quads*6+i]=quads*4+ix[i];quads++;
}
extern "C" void gl_app_init(){nano_run_constructors();}
extern "C" void gl_app_frame(int w,int h,uint32_t frame){
 if(w<=0||h<=0)return;
 if(!ready){
  if(!NanoInit(w,h)){errors++;return;}
  unloadedHeap=hb_os_heap_free();ready=true;
  for(unsigned i=0;i<8;i++)stateLow[i]=~0u;
 }
 // At a frame boundary the previous client-array draw has completed. Audit
 // every three full routes, without restarting the engine or resetting LRU totals.
 if(frame && frame%108==0){
  balanced=NanoUnload() && balanced;unloadedHeap=hb_os_heap_free();audits++;
  if(audits>1){
   if(!audioStats.heapLow||unloadedHeap<audioStats.heapLow)audioStats.heapLow=unloadedHeap;
   if(unloadedHeap>audioStats.heapHigh)audioStats.heapHigh=unloadedHeap;
   if(unloadedHeap<baselineLow)baselineLow=unloadedHeap;
   if(unloadedHeap>baselineHigh)baselineHigh=unloadedHeap;}
 }
 AudioTestTick();
 if(!NanoFrame(frame)){errors++;return;}
 while(glGetError()!=GL_NO_ERROR)errors++;
 loadedHeap=hb_os_heap_free();if(loadedHeap<heapLow)heapLow=loadedHeap;
 unsigned mask=0;for(unsigned i=0;i<StreamSlots;i++)if(stream.resident[i]>=0)mask|=1u<<stream.resident[i];
 if(frame>216){
  if(stateLow[mask]==~0u)stateLive[mask]=NanoLive();
  if(stateLive[mask]!=NanoLive())balanced=false;
  if(loadedHeap<stateLow[mask])stateLow[mask]=loadedHeap;
  if(loadedHeap>stateHigh[mask])stateHigh[mask]=loadedHeap;
  unsigned range=stateHigh[mask]-stateLow[mask];if(range>maxStateRange)maxStateRange=range;
 }
 balanced=balanced && !stream.failed && !NanoFailures() && NanoPeak()<HeapBudget;
 SetRenderStatePtr(TEXTURERASTER,nil);SetRenderState(ZTESTENABLE,0);SetRenderState(ZWRITEENABLE,0);SetRenderState(CULLMODE,CULLNONE);SetRenderState(VERTEXALPHA,0);
 quads=0;
 tile(10,350,25,12,errors?255:0,errors?0:255,0);
 tile(45,350,25,12,balanced?0:255,255,0);
 tile(80,350,25,12,gles1::unsupportedStates?255:0,255,0);
 // Machine-readable little-endian bit rows; schema is in verify.py.
 unsigned values[]={audioStats.blocks,audioStats.frames,audioStats.mixUs,audioStats.maxMixUs,audioStats.voicesPeak,audioStats.starts,audioStats.stops,audioStats.radioReads,audioStats.radioEnds,audioStats.failures,audioStats.live,audioStats.peak,audioStats.cycles,audioStats.heapLow,audioStats.heapHigh,frame};
 for(unsigned row=0;row<16;row++)for(unsigned i=0;i<32;i++){
  unsigned b=(values[row]>>i)&1;tile(8+i*7,368+row*4,5,3,b*255,b*255,b*255);
 }
 im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,overlay,quads*4,overlayIndices,quads*6);
}
