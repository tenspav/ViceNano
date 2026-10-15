#include "memory.h"
#include <assert.h>
#include <stdio.h>
using namespace rw;
bool loadModel();void unloadModel();void renderModel(unsigned);
extern unsigned mock_buffers_created,mock_buffers_deleted,mock_uploads,mock_deletes,mock_draws,mock_upload_bytes;
int main(){
 assert(Engine::init(&trackedMemory));registerMeshPlugin();registerNativeDataPlugin();EngineOpenParams p={240,432};assert(Engine::open(&p)&&Engine::start());
 Camera *c=Camera::create();Frame *f=Frame::create();c->setFrame(f);V3d pos={0,0,-5.5f};f->translate(&pos,COMBINEREPLACE);c->frameBuffer=Raster::create(240,432,0,Raster::CAMERA|Raster::DONTALLOCATE);
 V2d win={.5f*240/432,.5f};c->setViewWindow(&win);c->setNearPlane(1);c->setFarPlane(20);size_t baseline=rwLive;
 for(int i=0;i<100;i++){
  assert(loadModel());c->beginUpdate();renderModel(i*3);c->endUpdate();unloadModel();assert(rwLive==baseline);
  assert(Geometry::numAllocated==0 && Atomic::numAllocated==0 && Texture::numAllocated==0);
 }
 assert(mock_buffers_created==0 && mock_buffers_deleted==0 && mock_uploads==100 && mock_deletes==100 && mock_draws==100);
 // A separate RGBA4444 upload exercises the alpha format, in addition to the opaque VC cone.
 Raster *rgba=Raster::create(2,2,16,Raster::TEXTURE|Raster::C4444);assert(rgba);memset(rgba->lock(0,Raster::LOCKWRITE),255,8);rgba->unlock(0);rgba->destroy();assert(rwLive==baseline);
 printf("PASS: 100 model load/render/unload cycles; balanced textures/CPU vertex caches; RW baseline=%zu peak=%zu; uploaded texture bytes=%u; RGBA4444 upload accepted\n",baseline,rwPeak,mock_upload_bytes);
 c->frameBuffer->destroy();c->setFrame(nil);c->destroy();f->destroy();Engine::stop();Engine::close();Engine::term();assert(rwLive==0);
}
