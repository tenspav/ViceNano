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
uint32_t nanoStreamUs, nanoDrawUs;
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
 camera->setNearPlane(1);camera->setFarPlane(1000);V2d win={.5f*(float)w/h,.5f};camera->setViewWindow(&win);
 StreamReset();baseline=rwLive;return true;
}
bool NanoFrame(unsigned tick){
 if(!camera)return false;
 static bool positioned;
 if(!positioned){playerX=worldPacks[0].x;playerZ=worldPacks[0].z;positioned=true;}
 const CControllerState &p=pad.NewState;
 cameraYaw+=p.RightStickX/32767.f*2.f;cameraPitch+=p.RightStickY/32767.f;
 if(cameraPitch>40)cameraPitch=40;if(cameraPitch<-30)cameraPitch=-30;
 float yaw=cameraYaw*0.0174532925f,s=sinf(yaw),c=cosf(yaw);
 float speed=p.Cross?.24f:.08f;
 playerX+=(p.LeftStickX*c+p.LeftStickY*s)/32767.f*speed;
 playerZ+=(p.LeftStickY*c-p.LeftStickX*s)/32767.f*speed;
 V3d pos={playerX-s*140,worldPacks[0].y+25,playerZ-c*140};
 V3d tilt={1,0,0},up={0,1,0};camera->getFrame()->rotate(&tilt,6+cameraPitch,COMBINEREPLACE);
 camera->getFrame()->rotate(&up,cameraYaw,COMBINEPOSTCONCAT);
 camera->getFrame()->translate(&pos,COMBINEPOSTCONCAT);

 uint32_t t_s0 = hb_time_uptime_us();
 if(!StreamRequest(worldPacks,3,pos.x,pos.z))return false;
 nanoStreamUs = hb_time_uptime_us() - t_s0;

 uint32_t t_d0 = hb_time_uptime_us();
 RGBA bg={13,20,64,255};RwCameraClear(camera,&bg,Camera::CLEARIMAGE|Camera::CLEARZ);RwCameraBeginUpdate(camera);
 RwRenderStateSet(rwRENDERSTATEZTESTENABLE,(void*)1);RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,(void*)1);RwRenderStateSet(rwRENDERSTATECULLMODE,(void*)CULLBACK);
 SetRenderState(ALPHATESTFUNC,ALPHAALWAYS);
 gles1::applyStates();
 glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);
 m8ResetRenderState();
 StreamRender();
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
 RwCameraEndUpdate(camera);
 nanoDrawUs = hb_time_uptime_us() - t_d0;
 return true;
}

unsigned NanoFailures(){return allocationFailures;}
