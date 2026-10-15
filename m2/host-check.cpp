#include "../vendor/librw/rw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace rw;
extern unsigned mock_draws, mock_uploads, mock_deletes, mock_gl_calls;
extern float mock_projection[16],mock_clip[12];
static size_t live;
static void *alloc(size_t n,uint32){size_t *p=(size_t*)malloc(n+sizeof(size_t));if(!p)return nil;*p=n;live+=n;return p+1;}
static void dealloc(void *p){if(p){size_t *h=(size_t*)p-1;live-=*h;free(h);}}
static void *resize(void *p,size_t n,uint32 hint){void *q=alloc(n,hint);if(p&&q){memcpy(q,p,(*(size_t*)((char*)p-sizeof(size_t)))<n?(*(size_t*)((char*)p-sizeof(size_t))):n);dealloc(p);}return q;}
int main(){
 assert(!Engine::open(nil));MemoryFunctions mem={alloc,resize,dealloc,alloc,resize};
 assert(Engine::init(&mem));assert(!Engine::init(&mem));EngineOpenParams p={240,432};
 unsigned calls=mock_gl_calls;assert(Engine::open(&p));assert(Engine::start());assert(mock_gl_calls==calls);
 size_t baseline=live;
 Camera *c=Camera::create();Frame *f=Frame::create();c->setFrame(f);V3d pos={0,0,-5.5f};f->translate(&pos,COMBINEREPLACE);
 c->frameBuffer=Raster::create(240,432,0,Raster::CAMERA|Raster::DONTALLOCATE);
 V2d vw={.5f*240/432,.5f};c->setViewWindow(&vw);c->setNearPlane(1);c->setFarPlane(20);
 Raster *r=Raster::create(8,8,16,Raster::TEXTURE|Raster::C565);assert(r);memset(r->lock(0,Raster::LOCKWRITE),255,128);r->unlock(0);assert(mock_uploads==1);
 SetRenderStatePtr(TEXTURERASTER,r);SetRenderState(ZWRITEENABLE,0);assert(GetRenderState(ZWRITEENABLE)==0);assert(GetRenderStatePtr(TEXTURERASTER)==r);
 SetRenderState(TEXTUREADDRESS,Texture::CLAMP);assert(GetRenderState(TEXTUREADDRESSU)==Texture::CLAMP && GetRenderState(TEXTUREADDRESSV)==Texture::CLAMP);
 SetRenderState(FOGENABLE,1);assert(gles1::unsupportedStates==1 && !GetRenderState(FOGENABLE));
 c->beginUpdate();assert(c->devView.right.x==-1 && c->devView.pos.z==5.5f);assert(c->devProj.right.x>0 && ((float*)&c->devProj)[11]==1);
 gles1::Im3DVertex v[3]={};uint16 ix[]={0,1,2};Matrix m;m.setIdentity();
 im3d::Transform(v,3,&m,im3d::EVERYTHING);im3d::RenderIndexedPrimitive(PRIMTYPETRILIST,ix,3);im3d::End();
 gles1::Im2DVertex o[3]={};o[0].x=120;o[0].y=216;o[0].w=2;o[1].x=240;o[1].w=4;o[2].w=1;im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,o,3,ix,3);
 assert(mock_clip[0]==0 && mock_clip[1]==0 && mock_clip[3]==2 && mock_clip[4]==4 && mock_clip[5]==4 && mock_clip[7]==4);
 assert(mock_projection[11]==1); // Im2D restores positive-Z perspective.
 im3d::Transform(v,3,&m,im3d::EVERYTHING);im3d::RenderPrimitive(PRIMTYPETRILIST);im3d::End();assert(mock_draws==3);c->endUpdate();
 SetRenderStatePtr(TEXTURERASTER,nil);r->destroy();assert(mock_deletes==1);c->frameBuffer->destroy();c->setFrame(nil);c->destroy();f->destroy();
 assert(live==baseline);Engine::stop();Engine::close();Engine::term();fprintf(stderr,"live after term: %zu state %d\n",live,Engine::state);assert(live==0 && Engine::state==Engine::Dead);
 for(int i=0;i<3;i++){
  assert(Engine::init(&mem));assert(Engine::open(&p));assert(Engine::start());
  Engine::stop();Engine::close();Engine::term();assert(live==0);
 }
 puts("PASS: lifecycle, deferred GL, camera handedness/projection, state round trips, Im2D->Im3D restoration, raster resource release, zero RW allocation balance");
}
