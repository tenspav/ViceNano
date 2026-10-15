#include "../vendor/librw/rw.h"
extern "C" {
#include "hb_gl_surface.h"
#include "hb_heap.h"
#include "gl.h"
void nano_run_constructors();
}
#include "cube-data.h"
using namespace rw;
static Camera *camera;
static Raster *checker;
static gles1::Im3DVertex cube[24];
static unsigned errors,frames,baseHeap;
static bool ready;
static void overlay(int x,int y,int width,int height,RGBA c){
 gles1::Im2DVertex v[4]={};
 v[0].x=x;v[0].y=y;v[1].x=x+width;v[1].y=y;v[2].x=x+width;v[2].y=y+height;v[3].x=x;v[3].y=y+height;
 for(int i=0;i<4;i++){v[i].w=1;v[i].setColor(c.red,c.green,c.blue,c.alpha);}
 uint16 ix[]={0,1,2,0,2,3};im2d::RenderIndexedPrimitive(PRIMTYPETRILIST,v,4,ix,6);
}
extern "C" void gl_app_init(){nano_run_constructors();}
extern "C" void gl_app_frame(int w,int h,uint32_t frame){
 if(w<=0||h<=0)return;
 if(!ready){
  EngineOpenParams p={w,h};if(!Engine::init()||!Engine::open(&p)||!Engine::start()){glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);return;}
  camera=Camera::create();camera->setFrame(Frame::create());V3d pos={0,0,-5.5f};camera->getFrame()->translate(&pos,COMBINEREPLACE);
  camera->frameBuffer=Raster::create(w,h,0,Raster::CAMERA|Raster::DONTALLOCATE);
  camera->setNearPlane(1);camera->setFarPlane(20);V2d window={.5f*(float)w/h,.5f};camera->setViewWindow(&window);
  checker=Raster::create(8,8,16,Raster::TEXTURE|Raster::C565);
  uint16 *px=(uint16*)checker->lock(0,Raster::LOCKWRITE);for(int i=0;i<64;i++)px[i]=((i%8/2+i/8/2)&1)?0xffff:0xf800;checker->unlock(0);
  for(int i=0;i<24;i++){cube[i].position={vertices[i*3],vertices[i*3+1],vertices[i*3+2]};cube[i].setColor(255,255,255,255);cube[i].u=uv[2*i];cube[i].v=uv[2*i+1];}
  ready=true;
 }
 RGBA bg={13,20,64,255};camera->clear(&bg,Camera::CLEARIMAGE|Camera::CLEARZ);camera->beginUpdate();
 SetRenderState(ZTESTENABLE,1);SetRenderState(ZWRITEENABLE,1);SetRenderState(CULLMODE,CULLBACK);SetRenderState(VERTEXALPHA,0);
 SetRenderStatePtr(TEXTURERASTER,checker);SetRenderState(TEXTUREFILTER,Texture::NEAREST);
 Matrix m;m.setIdentity();V3d y={0,1,0},x={1,0,0};m.rotate(&x,25,COMBINEREPLACE);m.rotate(&y,frame%360,COMBINEPOSTCONCAT);
 im3d::Transform(cube,24,&m,im3d::EVERYTHING);im3d::RenderIndexedPrimitive(PRIMTYPETRILIST,(void*)indices,36);im3d::End();
 SetRenderStatePtr(TEXTURERASTER,nil);SetRenderState(ZTESTENABLE,0);SetRenderState(ZWRITEENABLE,0);SetRenderState(CULLMODE,CULLNONE);
 SetRenderState(VERTEXALPHA,1);overlay(10,360,100,25,makeRGBA(0,255,255,128));SetRenderState(VERTEXALPHA,0);
 while(glGetError()!=GL_NO_ERROR)errors++;
 if(frames==300)baseHeap=hb_os_heap_free();
 overlay(10,400,30,12,makeRGBA(errors?255:0,errors?0:255,0,255));
 overlay(50,400,30,12,makeRGBA(gles1::unsupportedStates?255:0,255,0,255));
 unsigned heap=hb_os_heap_free();for(unsigned i=0;i<32;i++){
  unsigned b=(heap>>i)&1;overlay(8+i*7,420,5,4,makeRGBA(b*255,b*255,b*255,255));
  b=(baseHeap>>i)&1;overlay(8+i*7,426,5,4,makeRGBA(b*255,b*255,b*255,255));
 }
 /* Scene after Im2D tests restoration of camera matrices and all render states. */
 SetRenderStatePtr(TEXTURERASTER,checker);SetRenderState(ZTESTENABLE,1);SetRenderState(ZWRITEENABLE,1);SetRenderState(CULLMODE,CULLBACK);
 V3d little={.16f,.16f,.16f},shift={1.1f,-1.7f,0};m.scale(&little,COMBINEPOSTCONCAT);m.translate(&shift,COMBINEPOSTCONCAT);
 im3d::Transform(cube,24,&m,im3d::EVERYTHING);im3d::RenderIndexedPrimitive(PRIMTYPETRILIST,(void*)indices,36);im3d::End();camera->endUpdate();frames++;
}
