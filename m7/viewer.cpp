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
#include "hb_touch_mb.h"
#include "hh_bt.h"
void nano_run_constructors();
}
using namespace rw;
static unsigned errors,loadedHeap,heapLow=~0u;

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
static void label(int x,int y,const char *text){
 for(;*text;text++,x+=6){const uint8_t *glyph=hb_font8x8_glyph(*text);
 for(int row=0;row<8;row++)for(int col=0;col<6;col++)if(glyph[row]&(0x80>>col))tile(y+row,x+col,1,1,220,230,240);}
}
extern "C" void gl_app_init(){nano_run_constructors();hb_touch_init();hh_bt_init();}
extern "C" void gl_app_frame(int w,int h,uint32_t frame){
 if(w<=0||h<=0)return;
 if(!ready){
  if(!NanoInit(w,h)){errors++;return;}
  ready=true;
 }
 hb_touch_t raw[HB_MAX_FINGERS];Contact contacts[8];int count=0;
 // RetailOS already converts digitizer values to screen pixels. Queue phases:
 // 0=down, 2=move, 1=up (the older hb_sdk.h comment swaps move/up).
 hb_touch_drain_all();int n=hb_touch_poll_multi(raw);
 static Contact held[8];static bool down[8];static uint32_t seenMs[8];
 uint32_t nowMs=hb_time_uptime_ms();bool seen[8]={};
 for(int i=0;i<n;i++)if(raw[i].touch_id<8){
 unsigned id=raw[i].touch_id;seen[id]=true;seenMs[id]=nowMs;down[id]=raw[i].status!=1;
 held[id]={int(id),raw[i].y,raw[i].x};
 }
 // The old snapshot can consume an up before polling. Bound a stale hold
 // when its queue disappears; newer residents supply an exact latched up.
 for(int i=0;i<8;i++)if(!seen[i]&&nowMs-seenMs[i]>1000)down[i]=false;
 hb_touch_state_t mailbox;
 if(hb_touch_mb_read(&mailbox)){
 down[0]=mailbox.down;held[0]={0,mailbox.y,mailbox.x};
 }
 // Empty queues mean no new sample, not finger-up. Latch until explicit up.
 for(int i=0;i<8;i++)if(down[i])contacts[count++]=held[i];
 TouchUpdate(contacts,count);
 // The reduced viewer uses Start as its explicit known-peer Connect action.
 static bool powerAsked;
 if(pad.NewState.Start&&!powerAsked){powerAsked=true;hh_bt_power_on();}
 hh_bt_tick();hid_pad hp;bool available=hh_bt_pad(&hp);
 // NanoPad's SDP descriptor names the right stick Z/Rz, not Rx/Ry.
 uint8_t peer[6];const uint8_t nanoPeer[6]={2,0x4e,0x50,0x41,0x44,1};
 if(available&&hh_bt_peer(peer)&&!memcmp(peer,nanoPeer,6)){
 hp.axis[3]=hp.axis[2];hp.axis[4]=hp.axis[5];hp.axis[2]=hp.axis[5]=0;
 }
 pad.Update(hh_bt_state()==HH_CONNECTED,available?&hp:0);
 if(!NanoFrame(frame)){errors++;return;}
 while(glGetError()!=GL_NO_ERROR)errors++;
 loadedHeap=hb_os_heap_free();if(loadedHeap<heapLow)heapLow=loadedHeap;

 balanced=balanced && !stream.failed && !NanoFailures() && NanoPeak()<HeapBudget;
 SetRenderStatePtr(TEXTURERASTER,nil);SetRenderState(ZTESTENABLE,0);SetRenderState(ZWRITEENABLE,0);SetRenderState(CULLMODE,CULLNONE);SetRenderState(VERTEXALPHA,0);
 quads=0;
 if(hh_bt_state()!=HH_CONNECTED){
 const char *names[]={"PAUSE","BACK","PRE","NXT","AIM","ENTER","FIRE","JUMP","RUN","DUCK"};
 for(int b=0;b<10;b++){const Button &v=buttons[b];bool pressed=reinterpret_cast<int16_t*>(&pad.NewState)[v.field]!=0;tile(v.y,v.x,v.h,v.w,pressed?240:50,pressed?180:75,110);label(v.x+3,v.y+v.h/2,names[b]);}
 tile(160,40,4,60,90,180,160);tile(135,20,50,4,90,180,160);
 }

 tile(10,130,25,12,errors?255:0,errors?0:255,0);
 tile(45,130,25,12,balanced?0:255,255,0);
 tile(80,130,25,12,gles1::unsupportedStates?255:0,255,0);
 // Machine-readable little-endian bit rows; schema is in verify.py.
 unsigned values[]={inputEvents,hh_bt_reports(),unsigned(int(playerX*16)),unsigned(int(playerZ*16)),unsigned(int(cameraYaw*100)),unsigned(int(cameraPitch*100)),unsigned(NanoLive()),loadedHeap,unsigned(NanoPeak()),errors,NanoFailures(),unsigned(hh_bt_state()),pad.NewState.Cross?1u:0u,unsigned(pad.NewState.LeftStickX),unsigned(pad.NewState.LeftStickY),frame};
 for(unsigned row=0;row<16;row++)for(unsigned i=0;i<32;i++){
  unsigned b=(values[row]>>i)&1;tile(8+i*7,148+row*4,5,3,b*255,b*255,b*255);
 }
 im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,overlay,quads*4,overlayIndices,quads*6);
}
