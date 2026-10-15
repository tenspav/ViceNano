#include "nano.h"
#include "memory.h"
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
using namespace rw;
#include "stream.h"
#include "../m5/generated/world.h"
#include "input.h"
#include <math.h>
float playerX,playerZ,cameraYaw,cameraPitch;
static Camera *camera;
static size_t baseline;
static bool opened, started, initialized;
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
 float speed=p.Cross?3.f:1.f;
 playerX+=(p.LeftStickX*c+p.LeftStickY*s)/32767.f*speed;
 playerZ+=(p.LeftStickY*c-p.LeftStickX*s)/32767.f*speed;
 V3d pos={playerX-s*140,worldPacks[0].y+25,playerZ-c*140};
 V3d tilt={1,0,0},up={0,1,0};camera->getFrame()->rotate(&tilt,6+cameraPitch,COMBINEREPLACE);
 camera->getFrame()->rotate(&up,cameraYaw,COMBINEPOSTCONCAT);
 camera->getFrame()->translate(&pos,COMBINEPOSTCONCAT);
 if(!StreamRequest(worldPacks,3,pos.x,pos.z))return false;
 RGBA bg={13,20,64,255};RwCameraClear(camera,&bg,Camera::CLEARIMAGE|Camera::CLEARZ);RwCameraBeginUpdate(camera);
 RwRenderStateSet(rwRENDERSTATEZTESTENABLE,(void*)1);RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,(void*)1);RwRenderStateSet(rwRENDERSTATECULLMODE,(void*)CULLBACK);
 SetRenderState(ALPHATESTFUNC,ALPHAALWAYS);StreamRender();
 RwCameraEndUpdate(camera);return true;
}

unsigned NanoFailures(){return allocationFailures;}
