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
