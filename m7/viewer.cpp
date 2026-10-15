#include "nano.h"
#include "stream.h"
#include "input.h"
extern float playerX,playerZ,cameraYaw,cameraPitch;
#include <string.h>
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
extern "C" {
#include "hb_gl_surface.h"
#include "hb_heap.h"
#include "gl.h"
#include "hb_sdk.h"
#include "hh_bt.h"
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
extern "C" void gl_app_init(){nano_run_constructors();hb_touch_init();hh_bt_init();}
extern "C" void gl_app_frame(int w,int h,uint32_t frame){
 if(w<=0||h<=0)return;
 if(!ready){
  if(!NanoInit(w,h)){errors++;return;}
  unloadedHeap=hb_os_heap_free();ready=true;
  for(unsigned i=0;i<8;i++)stateLow[i]=~0u;
 }
 hb_touch_t raw[HB_MAX_FINGERS];Contact contacts[8];int count=0;
 hb_touch_drain_all();int n=hb_touch_poll_multi(raw);
 for(int i=0;i<n;i++)if(raw[i].status!=2){contacts[count++]={int(raw[i].touch_id),raw[i].y,239-raw[i].x};}
 TouchUpdate(contacts,count);
 // Give touch verification an initial window; then exercise outgoing secure HID.
 if(frame==650)hh_bt_power_on();
 hh_bt_tick();hid_pad hp;bool available=hh_bt_pad(&hp);
 pad.Update(hh_bt_state()==HH_CONNECTED,available?&hp:0);
 if(!NanoFrame(frame)){errors++;return;}
 while(glGetError()!=GL_NO_ERROR)errors++;
 loadedHeap=hb_os_heap_free();if(loadedHeap<heapLow)heapLow=loadedHeap;
 unsigned mask=0;for(unsigned i=0;i<StreamSlots;i++)if(stream.resident[i]>=0)mask|=1u<<stream.resident[i];
 balanced=balanced && !stream.failed && !NanoFailures() && NanoPeak()<HeapBudget;
 SetRenderStatePtr(TEXTURERASTER,nil);SetRenderState(ZTESTENABLE,0);SetRenderState(ZWRITEENABLE,0);SetRenderState(CULLMODE,CULLNONE);SetRenderState(VERTEXALPHA,0);
 quads=0;
 if(hh_bt_state()!=HH_CONNECTED){
 for(int b=0;b<10;b++){const Button &v=buttons[b];bool pressed=reinterpret_cast<int16_t*>(&pad.NewState)[v.field]!=0;tile(239-v.y-v.h,v.x,v.h,v.w,pressed?240:50,pressed?180:75,110);}
 tile(239-160,40,4,60,90,180,160);tile(239-135,20,50,4,90,180,160);
 }
 tile(116,205,8,12,255,220,0);
 tile(10,350,25,12,errors?255:0,errors?0:255,0);
 tile(45,350,25,12,balanced?0:255,255,0);
 tile(80,350,25,12,gles1::unsupportedStates?255:0,255,0);
 // Machine-readable little-endian bit rows; schema is in verify.py.
 unsigned values[]={inputEvents,hh_bt_reports(),unsigned(int(playerX*16)),unsigned(int(playerZ*16)),unsigned(int(cameraYaw*100)),unsigned(int(cameraPitch*100)),unsigned(NanoLive()),loadedHeap,unsigned(NanoPeak()),errors,NanoFailures(),unsigned(hh_bt_state()),pad.NewState.Cross?1u:0u,unsigned(pad.NewState.LeftStickX),unsigned(pad.NewState.LeftStickY),frame};
 for(unsigned row=0;row<16;row++)for(unsigned i=0;i<32;i++){
  unsigned b=(values[row]>>i)&1;tile(8+i*7,368+row*4,5,3,b*255,b*255,b*255);
 }
 im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,overlay,quads*4,overlayIndices,quads*6);
}
