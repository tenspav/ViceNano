#include "nano.h"
#include "memory.h"
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
using namespace rw;
bool loadModel();void unloadModel();void renderModel(unsigned);
static Camera *camera;
static size_t baseline;
static unsigned cycles;
static bool loaded, opened, started, initialized;
size_t NanoLive(){return rwLive;}size_t NanoPeak(){return rwPeak;}
size_t NanoBaseline(){return baseline;}unsigned NanoCycles(){return cycles;}
bool NanoUnload(){
 if(loaded){unloadModel();loaded=false;}
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
 camera->setFrame(f);V3d pos={0,0,-5.5f};f->translate(&pos,COMBINEREPLACE);
 camera->frameBuffer=Raster::create(w,h,0,Raster::CAMERA|Raster::DONTALLOCATE);
 if(!camera->frameBuffer){NanoExit();return false;}
 camera->setNearPlane(1);camera->setFarPlane(20);V2d win={.5f*(float)w/h,.5f};camera->setViewWindow(&win);
 baseline=rwLive;return true;
}
bool NanoFrame(unsigned tick){
 if(!camera)return false;
 if(!loaded){loaded=loadModel();if(!loaded)return false;cycles++;}
 RGBA bg={13,20,64,255};RwCameraClear(camera,&bg,Camera::CLEARIMAGE|Camera::CLEARZ);RwCameraBeginUpdate(camera);
 RwRenderStateSet(rwRENDERSTATEZTESTENABLE,(void*)1);RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,(void*)1);RwRenderStateSet(rwRENDERSTATECULLMODE,(void*)CULLBACK);
 SetRenderState(ALPHATESTFUNC,ALPHAALWAYS);renderModel(tick%360);
 RwCameraEndUpdate(camera);return true;
}
