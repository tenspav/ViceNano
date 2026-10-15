#include "nano.h"
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
extern "C" {
#include "hb_gl_surface.h"
#include "hb_heap.h"
#include "gl.h"
void nano_run_constructors();
}
using namespace rw;
bool loadModel();void unloadModel();void renderModel(unsigned);
static Camera *camera;
static unsigned errors,cycles,cpuBaseline,heapLow=~0u,heapHigh,loadedHeap,unloadedHeap;
static bool ready,balanced=true,loaded;
static void tile(int x,int y,int w,int h,unsigned r,unsigned g,unsigned b){
 gles1::Im2DVertex v[4]={};v[0].x=x;v[0].y=y;v[1].x=x+w;v[1].y=y;v[2].x=x+w;v[2].y=y+h;v[3].x=x;v[3].y=y+h;
 for(int i=0;i<4;i++){v[i].w=1;v[i].setColor(r,g,b,255);}uint16 ix[]={0,1,2,0,2,3};im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,v,4,ix,6);
}
extern "C" void gl_app_init(){nano_run_constructors();}
extern "C" void gl_app_frame(int w,int h,uint32_t frame){
 if(w<=0||h<=0)return;
 if(!ready){
  if(!NanoInit(w,h)){errors++;return;}
  cpuBaseline=NanoBaseline();unloadedHeap=hb_os_heap_free();ready=true;
 }
 if(frame && frame%180==0){
  balanced=NanoExit() && balanced;ready=false;
  balanced=NanoLive()==0 && balanced;
  if(!NanoInit(w,h)){errors++;return;}ready=true;cpuBaseline=NanoBaseline();
  unloadedHeap=hb_os_heap_free();
 }
 if(!NanoFrame(frame)){errors++;return;}cycles=NanoCycles();
 while(glGetError()!=GL_NO_ERROR)errors++;
 loadedHeap=hb_os_heap_free();if(frame>300){if(loadedHeap<heapLow)heapLow=loadedHeap;if(loadedHeap>heapHigh)heapHigh=loadedHeap;}
 SetRenderStatePtr(TEXTURERASTER,nil);SetRenderState(ZTESTENABLE,0);SetRenderState(ZWRITEENABLE,0);SetRenderState(CULLMODE,CULLNONE);SetRenderState(VERTEXALPHA,0);
 tile(10,400,25,12,errors?255:0,errors?0:255,0);tile(45,400,25,12,balanced?0:255,255,0);tile(80,400,25,12,gles1::unsupportedStates?255:0,255,0);
 for(unsigned i=0;i<8;i++){unsigned b=(cycles>>i)&1;tile(112+i*15,400,10,12,b*255,b*255,b*255);}
 for(unsigned i=0;i<32;i++){
  unsigned b=(cpuBaseline>>i)&1;tile(8+i*7,412,5,4,b*255,b*255,b*255);b=(unloadedHeap>>i)&1;tile(8+i*7,416,5,4,b*255,b*255,b*255);
  b=(NanoLive()>>i)&1;tile(8+i*7,420,5,4,b*255,b*255,b*255);b=(loadedHeap>>i)&1;tile(8+i*7,426,5,4,b*255,b*255,b*255);
 }

}
