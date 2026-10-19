#ifdef M8_NANOOS
#include "nano.h"
#include "nanoos_shim.h"
#include "model.h"
extern "C" {
#include "lcd.h"
#include "fmt.h"
#include "bt.h"
#include "touch.h"
}
// Bluetooth gamepad (nanoos/src/bt.c): weak, the slim kernel has no bt.o
#pragma weak bt_pad_poll
bool m8PadExit;   // Start/PS/Home pressed on the pad: m8_app_tick leaves to the menu
int m8_world_draw(const M8Cam &cam);
bool m8_world_init(bool vc);
void m8_world_shutdown();
static bool initialized, gpu, worldReady, worldVc, vcMissing;
static unsigned mode,ctl,seen;
static int testTexture;
static uint16_t quadPixels[64];
void m8_render_mode(unsigned value){value%=M8_MODES;if(value!=mode)ctl=0;mode=value;}

float playerX,playerZ,cameraYaw,cameraPitch;
// Mode 4 free camera at street level, integer state (soft-float code is
// large and the kernel is at its 64 KiB cap): eye camX, EYE_Y, camZ in 1/1024
// m (vcworld.bin is in metres; the eye follows the ground of the nearest
// cell, m8VcGround, the roads rise ~6 m); heading hdg in millidegrees,
// 0 = +z, Vol+ turns right. Home cycles ctl: 0 walk (Vol+/- move along the
// heading), 1 turn, 2 scene (Vol+/- change the mode as elsewhere). Held
// buttons act continuously; a press too short to be seen held (it ends
// between two ticks) is one TAP_US step. Touch drag (round 8) looks around:
// x turns the heading (drag right = turn right), y the pitch pit (millidegrees
// down, PIT_MIN..PIT_MAX); the gesture itself is dropped in this scene.
#define EYE_Y 1.7f
#define TAP_US 200000      // walk 8 m/s (the block is ~250 m across), turn 90 deg/s
#define PIT_MIN (-15000)
#define PIT_MAX 30000
static int camX,camZ,hdg,pit,touchX,touchY;
static bool touchOn;
// drag look: deltas between successive touch samples of one contact (a DOWN
// only anchors; a sample seen twice moves nothing), 0.4 deg/px yaw, 0.25
// pitch; returns the yaw step (millidegrees)
static int m8_touch_look(){
 int d=0;
 if(touchOn&&touch.state!=TOUCH_DOWN){
  d=(touch.x-touchX)*400;
  pit+=(touch.y-touchY)*250;pit=pit<PIT_MIN?PIT_MIN:pit>PIT_MAX?PIT_MAX:pit;
 }
 touchOn=touch.state!=TOUCH_UP;touchX=touch.x;touchY=touch.y;
 return d;
}
static uint32_t lastUs,frameUs,fps10;
// Bhaskara I sine * 1024 of whole degrees, d in [0,360)
static int sin1k(int d){int x=d%180,p=x*(180-x),r=4096*p/(40500-p);return d<180?r:-r;}
static int clamp(int v){return v>150*1024?150*1024:v<-150*1024?-150*1024:v;} // stay near the block
bool m8_walk_home(){if(mode!=4||vcMissing)return false;ctl=(ctl+1)%3;return true;}
// Pad (any ctl): left stick or d-pad, forward/back walks along the heading
// at up to 8 m/s, left/right turns at up to 90 deg/s, 16/127 dead zone;
// cross (PAD_S) = action, a placeholder; a Start/PS/Home press exits.
// Vol+/- keep working, with or without a pad (BT starts on the first
// mode 4 frame: the power-up blocks ~0.2-0.5 s once).
static uint32_t padPrev;
static void m8_pad(uint32_t dt){
 bt_pad p;
 if(!bt_pad_poll||!bt_pad_poll(&p)){padPrev=0;return;}
 if(p.buttons&~padPrev&(PAD_START|PAD_HOME))m8PadExit=true;
 padPrev=p.buttons;
 int f=-p.ly,r=p.lx;
 if(p.buttons&PAD_UP)f=127;
 if(p.buttons&PAD_DOWN)f=-127;
 if(p.buttons&PAD_LEFT)r=-127;
 if(p.buttons&PAD_RIGHT)r=127;
 if(f<16&&f>-16)f=0;
 if(r<16&&r>-16)r=0;
 if(p.buttons&PAD_S){} // action: nothing to do yet
 hdg=(hdg+r*(int)dt/127*9/100+360000)%360000;
 int us=f*(int)dt/127,h=hdg/1000;
 camX=clamp(camX+sin1k(h)*us/125000);camZ=clamp(camZ+sin1k((h+90)%360)*us/125000);
}
bool m8_walk(unsigned held,unsigned edges){
 uint32_t t=m8_time_us(),dt=t-lastUs;lastUs=t;if(dt>250000)dt=250000;
 if(mode!=4||vcMissing){seen=0;touchOn=false;return false;}
 m8_pad(dt);
 int look=m8_touch_look(),us=0,h=hdg/1000; // drag looks in every ctl
 if(ctl==2)seen=0;
 else{
  held&=6;if(held==6)held=0; // the chord is the exit, not a step
  unsigned tap=edges&6&~seen;seen=held;
  us=((int)(held>>2&1)-(int)(held>>1&1))*(int)dt+((int)(tap>>2&1)-(int)(tap>>1&1))*TAP_US;
 }
 if(ctl==1)look+=us*9/100;
 hdg=(hdg+look+720000)%360000;
 if(!ctl){camX=clamp(camX+sin1k(h)*us/125000);camZ=clamp(camZ+sin1k((h+90)%360)*us/125000);}
 return ctl!=2;
}
uint32_t nanoStreamUs,nanoDrawUs,nanoEndUs,nanoClearUs,nanoBeginUs,nanoWorldUs;
uint32_t nanoBenchElementsX10,nanoBenchArraysX10;
bool NanoInit(int w,int h){if(initialized||w!=432||h!=240)return false;initialized=true;mode=0;
 // on nb_road02 between the deco shop (left) and the hotel, facing north
 camX=35*1024;camZ=50*1024;hdg=180000;pit=3000;gpu=m8_gpu_init()==0;
if(gpu){for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)quadPixels[y*8+x]=y<4?(x<4?0xf800:0x07e0):(x<4?0x001f:0xffe0);testTexture=m8_gpu_texture_create(8,8,1,quadPixels);worldReady=m8_world_init(false);worldVc=false;}
return true;}
bool NanoFrame(unsigned tick){
 if(!initialized)return false;
 if(!mode){m8_title(tick);return true;}
 // Mode 3 is the embedded preview, 4 the Vice City street corner from the
 // asset pack; both use block 0, swapped here, outside a scene (uploads).
 if(gpu&&mode>=3&&(mode==4)!=worldVc){
  m8_world_shutdown();worldVc=mode==4;
  worldReady=worldVc&&m8_world_init(true);
  vcMissing=worldVc&&!worldReady;
  if(!worldReady)worldReady=m8_world_init(false);
 }
 // modes 3/4 read vcworld.pak (preview.bin, vcworld.bin): none, no scene
 if(mode>=3&&!worldReady){
  lcd_clear(0xff000000);lcd_center(110,"no world pack: install vcworld.pak",WHITE);
  m8_footer();lcd_present();return true;
 }
 m8_gpu_direct=mode==4&&!vcMissing; // walk: no sentinel/copy (nanoos_gpu_mbx.cpp)
 if(!gpu || m8_gpu_begin(0xff0d1440)<0){m8_fail_screen();return true;}
 if(mode==1){
  const M8GpuVertex v[3]={{120,170,.5f,0xffff0000},{216,50,.5f,0xff00ff00},{312,170,.5f,0xff0000ff}};
  if(m8_gpu_triangle(v)<0)return false;
 }else if(mode==2){
  const M8GpuTexVertex v[4]={{{120,64,0,0xffffffff},0,0},{{312,64,0,0xffffffff},1,0},{{312,192,0,0xffffffff},1,1},{{120,192,0,0xffffffff},0,1}};
  const M8GpuTexVertex a[3]={v[0],v[1],v[2]},b[3]={v[0],v[2],v[3]};
  if(m8_gpu_textured_triangle(testTexture,a)<0||m8_gpu_textured_triangle(testTexture,b)<0)return false;
 }else{
  static const M8Cam preview={0,12,0,.70710678f,.70710678f,.97f,.24f,175,8,false};
  const int h=hdg/1000;const float k=1/1024.f;
  static float eye=EYE_Y;
  eye+=(m8VcGround(camX*k,camZ*k,eye-EYE_Y)+EYE_Y-eye)*.5f; // eases over steps
  const int p=(pit/1000+360)%360; // pitch, whole degrees down (3 at start)
  const M8Cam walk={camX*k,eye,camZ*k,sin1k(h)*k,sin1k((h+90)%360)*k,sin1k((p+90)%360)*k,sin1k(p)*k,0,1,true};
  if(!worldReady||m8_world_draw(mode==4&&!vcMissing?walk:preview)<0)return false;
 }
 // A failed frame is reported at once (first one per session kept in R80),
 // then the MBX is re-initialised so the preview keeps going. When
 // m8_gpu_recover() has used its re-inits, R80 is finalized before giving up.
 if(m8_gpu_end()<0){
  m8_diag_report("frame failed");gpu=m8_gpu_recover()==0;
  if(!gpu)m8_gpu_gave_up("gave up");
  return false;
 }
 if(mode==4&&!vcMissing){ // HUD: control, eye position, heading, frame rate
  uint32_t t=m8_time_us();if(frameUs)fps10=10000000u/(t-frameUs+1);frameUs=t;
  fmt_str(ctl==0?"WALK x ":ctl==1?"TURN x ":"SCENE x ");fmt_sdec(camX>>10);fmt_str(" z ");fmt_sdec(camZ>>10);
  fmt_str(" hdg ");fmt_dec(hdg/1000);fmt_str(" fps ");fmt_dec(fps10/10);fmt_str(".");fmt_dec(fps10%10);fmt_str(" t ");fmt_dec(m8Stats.drawCalls);
  fmt_line(8,206,WHITE);
 }
 lcd_center(16,mode==1?"MBX triangle":mode==2?"MBX texture test":mode==3?"MBX world preview":vcMissing?"no VC pack: preview":"Vice City: North Beach",WHITE);
 // walking: the scene stays clear unless the GPU reported trouble (R80 has all)
 if(mode!=4||m8_gpu_status.timeouts||m8_gpu_status.errors)m8_diag_render_draw(8,28,WHITE);
 m8_footer();
 lcd_present();return true;
}
bool NanoUnload(){m8_world_shutdown();worldReady=false;return true;}
bool NanoExit(){NanoUnload();m8_gpu_texture_destroy(testTexture);testTexture=0;m8_gpu_shutdown();initialized=gpu=false;return true;}
size_t NanoLive(){return 0;} size_t NanoPeak(){return 0;} size_t NanoBaseline(){return 0;}
unsigned NanoCycles(){return 0;} unsigned NanoFailures(){return 0;}
void NanoSetHardwareUnlock(bool){}
#else
#include "nano.h"
#include "memory.h"
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
using namespace rw;
#include "stream.h"
#include "generated/world.h"
#include "input.h"
#include <math.h>

extern "C" {
#include "hb_sdk.h"
#include "gl.h"
}

float playerX,playerZ,cameraYaw,cameraPitch;
uint32_t nanoStreamUs, nanoDrawUs, nanoEndUs, nanoClearUs, nanoBeginUs, nanoWorldUs;
uint32_t nanoBenchElementsX10, nanoBenchArraysX10;
static Camera *camera;
static size_t baseline;
static bool opened, started, initialized;
static bool s_unlockHardware = true;

void NanoSetHardwareUnlock(bool enable) {
 s_unlockHardware = enable;
}

static void applyHardwareConfig() {
 // Opt current RetailOS task into lazy VFP context switching (sets FPSCR to 0x03000f00)
 hb_fpu_attach_current_task(NULL);
}

size_t NanoLive(){return rwLive;}size_t NanoPeak(){return rwPeak;}
size_t NanoBaseline(){return baseline;}unsigned NanoCycles(){return stream.loads;}
bool NanoUnload(){
 StreamClear();
 return rwLive==baseline && Geometry::numAllocated==0 && Atomic::numAllocated==0 && Texture::numAllocated==0 && Clump::numAllocated==0;
}
bool NanoExit(){
 if(!initialized)return rwLive==0;
 bool balanced=NanoUnload();
 if(camera){Frame *f=camera->getFrame();if(camera->frameBuffer)camera->frameBuffer->destroy();camera->setFrame(nil);camera->destroy();f->destroy();camera=nil;}
 if(started)Engine::stop();if(opened)Engine::close();Engine::term();
 initialized=opened=started=false;baseline=0;
 return balanced && rwLive==0;
}
bool NanoInit(int w,int h){
 if(initialized||w<=0||h<=0)return false;
 applyHardwareConfig();
 if(!Engine::init(&trackedMemory))return false;initialized=true;
 registerMeshPlugin();registerNativeDataPlugin();EngineOpenParams p={w,h};
 if(!(opened=Engine::open(&p))||!(started=Engine::start())){NanoExit();return false;}
 camera=Camera::create();if(!camera){NanoExit();return false;}
 Frame *f=Frame::create();if(!f){camera->destroy();camera=nil;NanoExit();return false;}
 camera->setFrame(f);V3d pos={0,0,0};f->translate(&pos,COMBINEREPLACE);
 camera->frameBuffer=Raster::create(w,h,0,Raster::CAMERA|Raster::DONTALLOCATE);
 if(!camera->frameBuffer){NanoExit();return false;}
 camera->setNearPlane(1);camera->setFarPlane(380);V2d win={.5f*(float)w/h,.5f};camera->setViewWindow(&win);
 StreamReset();baseline=rwLive;return true;
}
// Times N one-triangle draws through each submission path. The indexed path
// round-trips through the RetailOS MBX flush thread on every call.
static void benchDrawPaths(){
 static gles1::Im3DVertex tri[3];
 static const uint16_t ix[3]={0,1,2};
 for(int i=0;i<3;i++){tri[i].position={playerX+(i==1?1.f:0.f),worldPacks[0].y+(i==2?1.f:0.f),playerZ};tri[i].setColor(255,220,0,255);}
 glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);
 glVertexPointer(3,GL_FLOAT,sizeof(gles1::Im3DVertex),&tri[0].position);
 glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(gles1::Im3DVertex),(uint8*)tri+offsetof(gles1::Im3DVertex,r));
 glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 const unsigned n=32;
 uint32_t t0=hb_time_uptime_us();
 for(unsigned i=0;i<n;i++)glDrawArrays(GL_TRIANGLES,0,3);
 uint32_t t1=hb_time_uptime_us();
 for(unsigned i=0;i<n;i++)glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,ix);
 uint32_t t2=hb_time_uptime_us();
 glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 nanoBenchArraysX10=(t1-t0)*10/n;nanoBenchElementsX10=(t2-t1)*10/n;
}

#ifdef M8_PROFILE
// {magic, magic^stamp, phase, stamp}: NanoEmu finds it by scanning DRAM. It lives in
// .bss and is filled at run time, so no copy of the image matches.
static volatile uint32_t m8Prof[4];
void m8ProfPhase(uint32_t phase){
 if(!m8Prof[3]){uint32_t t=hb_time_uptime_us()|1;m8Prof[3]=t;m8Prof[1]=0x4d385052u^t;m8Prof[0]=0x4d385052u;}
 m8Prof[2]=phase;
}
#endif

bool NanoFrame(unsigned tick){
 if(!camera)return false;
 static bool positioned;
 if(!positioned){playerX=worldPacks[0].x;playerZ=worldPacks[0].z;positioned=true;}
 // Movement is per elapsed time, normalised to the 20 Hz baseline step, so runs at
 // different frame rates cover comparable routes.
 static uint32_t lastUs;
 uint32_t nowUs=hb_time_uptime_us();
 float step=lastUs?(nowUs-lastUs)/50000.f:1.f;
 if(step>3.f)step=3.f;
 lastUs=nowUs;
 const CControllerState &p=pad.NewState;
 cameraYaw+=p.RightStickX/32767.f*2.f*step;cameraPitch+=p.RightStickY/32767.f*step;
 if(cameraPitch>40)cameraPitch=40;if(cameraPitch<-30)cameraPitch=-30;
 float yaw=cameraYaw*0.0174532925f,s=sinf(yaw),c=cosf(yaw);
 float speed=(p.Cross?.24f:.08f)*step;
 playerX+=(p.LeftStickX*c+p.LeftStickY*s)/32767.f*speed;
 playerZ+=(p.LeftStickY*c-p.LeftStickX*s)/32767.f*speed;
 V3d pos={playerX-s*140,worldPacks[0].y+25,playerZ-c*140};
 V3d tilt={1,0,0},up={0,1,0};camera->getFrame()->rotate(&tilt,6+cameraPitch,COMBINEREPLACE);
 camera->getFrame()->rotate(&up,cameraYaw,COMBINEPOSTCONCAT);
 camera->getFrame()->translate(&pos,COMBINEPOSTCONCAT);

 m8ProfPhase(M8P_STREAM);
 uint32_t t_s0 = hb_time_uptime_us();
 if(!StreamRequest(worldPacks,3,pos.x,pos.z)||!m8PrepareBatches())return false;
 nanoStreamUs = hb_time_uptime_us() - t_s0;

 m8ProfPhase(M8P_BEGIN);
 uint32_t t_d0 = hb_time_uptime_us();
 RGBA bg={13,20,64,255};RwCameraClear(camera,&bg,Camera::CLEARIMAGE|Camera::CLEARZ);
 uint32_t t_b0 = hb_time_uptime_us();
 nanoClearUs = t_b0 - t_d0;
 RwCameraBeginUpdate(camera);
 uint32_t t_w0 = hb_time_uptime_us();
 nanoBeginUs = t_w0 - t_b0;
 m8ProfPhase(M8P_WORLD);
 m8ResetRenderState();
 if(tick==90)benchDrawPaths();
 StreamRender();
 m8ProfPhase(M8P_END);
 nanoWorldUs = hb_time_uptime_us() - t_w0;
 // A world-space yellow diamond is the reduced player marker (no ped/physics).
 static gles1::Im3DVertex marker[6];
 const float dx[6]={-4,0,4,-4,4,0},dy[6]={0,12,0,0,0,-4};
 for(int i=0;i<6;i++){marker[i].position={playerX+dx[i],worldPacks[0].y+dy[i],playerZ};marker[i].setColor(255,220,0,255);}
 glDisable(GL_TEXTURE_2D);
 glDisable(GL_BLEND);
 glDisable(GL_CULL_FACE);
 glVertexPointer(3, GL_FLOAT, sizeof(gles1::Im3DVertex), &marker[0].position);
 glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(gles1::Im3DVertex), (uint8*)marker + offsetof(gles1::Im3DVertex, r));
 glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 glDrawArrays(GL_TRIANGLES, 0, 6);
 glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 uint32_t t_e0 = hb_time_uptime_us();
 RwCameraEndUpdate(camera);
 nanoEndUs = hb_time_uptime_us() - t_e0;
 nanoDrawUs = hb_time_uptime_us() - t_d0;
 m8ProfPhase(M8P_FRAME);
 return true;
}

unsigned NanoFailures(){return allocationFailures;}

#endif // M8_NANOOS
