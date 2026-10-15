/* Resident owns context and present; engine lifecycle never creates an EGL window. */
#include "gles1impl.h"
#include <string.h>
namespace rw { namespace gles1 {
float view[16],projection[16];
int width=240,height=432;
uint32 unsupportedStates;
static uintptr states[GSALPHATESTREF+1];
static bool active;
static void toggle(GLenum cap,bool on){if(on)glEnable(cap);else glDisable(cap);}
static GLenum blend(uintptr x){
 static const GLenum map[]={GL_ZERO,GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,
 GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,
 GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE};
 return x<=BLENDSRCALPHASAT?map[x]:GL_ONE;
}
void applyStates(){
 toggle(GL_DEPTH_TEST,states[ZTESTENABLE]);glDepthMask(states[ZWRITEENABLE]!=0);glDepthFunc(GL_LEQUAL);
 toggle(GL_CULL_FACE,states[CULLMODE]!=CULLNONE);
 glCullFace(states[CULLMODE]==CULLFRONT?GL_FRONT:GL_BACK);
 toggle(GL_BLEND,states[VERTEXALPHA]);glBlendFunc(blend(states[SRCBLEND]),blend(states[DESTBLEND]));
 toggle(GL_ALPHA_TEST,states[ALPHATESTFUNC]!=ALPHAALWAYS);
 glAlphaFunc(states[ALPHATESTFUNC]==ALPHALESS?GL_LESS:GL_GEQUAL,(float)states[ALPHATESTREF]/255);
 glDisable(GL_LIGHTING);
 Raster *r=(Raster*)states[TEXTURERASTER];
 toggle(GL_TEXTURE_2D,r!=nil);
 if(r){
  glBindTexture(GL_TEXTURE_2D,nativeRaster(r)->texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,states[TEXTUREFILTER]==Texture::NEAREST?GL_NEAREST:GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,states[TEXTUREFILTER]==Texture::NEAREST?GL_NEAREST:GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,states[TEXTUREADDRESSU]==Texture::CLAMP?GL_CLAMP_TO_EDGE:GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,states[TEXTUREADDRESSV]==Texture::CLAMP?GL_CLAMP_TO_EDGE:GL_REPEAT);
 }
}
static void setState(int32 s,void *v){
 if(s<0 || s>GSALPHATESTREF){unsupportedStates++;return;}
 if(s==TEXTUREADDRESS){states[TEXTUREADDRESSU]=states[TEXTUREADDRESSV]=(uintptr)v;}
 if(s==FOGENABLE || (s>=STENCILENABLE && s<=STENCILFUNCTIONWRITEMASK) || s>=GSALPHATEST){
  if(v)unsupportedStates++;return;
 }
 states[s]=(uintptr)v;
}
static void *getState(int32 s){return s>=0 && s<=GSALPHATESTREF?(void*)states[s]:nil;}
void loadWorld(Matrix *world){
 float m[16];glMatrixMode(GL_MODELVIEW);glLoadMatrixf(view);
 if(!world)return;
 m[0]=world->right.x;m[1]=world->right.y;m[2]=world->right.z;m[3]=0;
 m[4]=world->up.x;m[5]=world->up.y;m[6]=world->up.z;m[7]=0;
 m[8]=world->at.x;m[9]=world->at.y;m[10]=world->at.z;m[11]=0;
 m[12]=world->pos.x;m[13]=world->pos.y;m[14]=world->pos.z;m[15]=1;
 glMultMatrixf(m);
}
void restore3D(){glMatrixMode(GL_PROJECTION);glLoadMatrixf(projection);loadWorld(nil);}
static void begin(Camera *cam){
 Matrix inv;Matrix::invert(&inv,cam->getFrame()->getLTM());
 width=cam->frameBuffer->width;height=cam->frameBuffer->height;
 glViewport(0,0,width,height);
 /* RW looks into positive Z and flips X; port exactly the GL3 camera convention. */
 float a[]={-inv.right.x,inv.right.y,inv.right.z,0,-inv.up.x,inv.up.y,inv.up.z,0,
 -inv.at.x,inv.at.y,inv.at.z,0,-inv.pos.x,inv.pos.y,inv.pos.z,1};
 memcpy(view,a,sizeof(view));memset(projection,0,sizeof(projection));
 float iz=1/(cam->farPlane-cam->nearPlane);
 projection[0]=1/cam->viewWindow.x;projection[5]=1/cam->viewWindow.y;
 projection[8]=cam->viewOffset.x/cam->viewWindow.x;
 projection[9]=cam->viewOffset.y/cam->viewWindow.y;
 projection[12]=-projection[8];projection[13]=-projection[9];
 if(cam->projection==Camera::PERSPECTIVE){projection[10]=(cam->farPlane+cam->nearPlane)*iz;
 projection[11]=1;projection[14]=-2*cam->nearPlane*cam->farPlane*iz;}
 else{projection[10]=2*iz;projection[14]=-(cam->farPlane+cam->nearPlane)*iz;projection[15]=1;}
 memcpy(&cam->devView,view,64);memcpy(&cam->devProj,projection,64);restore3D();active=true;
}
static void end(Camera*){active=false;}
static void clear(Camera*,RGBA *c,uint32 mode){
 glDepthMask(GL_TRUE);glClearColor(c->red/255.f,c->green/255.f,c->blue/255.f,c->alpha/255.f);
 glClearDepthf(1);glClear(((mode&Camera::CLEARIMAGE)?GL_COLOR_BUFFER_BIT:0)|((mode&Camera::CLEARZ)?GL_DEPTH_BUFFER_BIT:0));
}
static void show(Raster*,uint32){}
static bool32 blit(Raster*,int32,int32){return 0;}
static int system(DeviceReq req,void *p,int32 n){
 switch(req){
 case DEVICEOPEN: if(p){width=((EngineOpenParams*)p)->width;height=((EngineOpenParams*)p)->height;}return 1;
 case DEVICEINIT:
  memset(states,0,sizeof(states));states[ZTESTENABLE]=states[ZWRITEENABLE]=1;states[CULLMODE]=CULLBACK;
  states[SRCBLEND]=BLENDSRCALPHA;states[DESTBLEND]=BLENDINVSRCALPHA;
  states[TEXTUREFILTER]=Texture::LINEAR;states[TEXTUREADDRESS]=states[TEXTUREADDRESSU]=states[TEXTUREADDRESSV]=Texture::WRAP;
  unsupportedStates=0;return 1;
 case DEVICEFINALIZE:case DEVICETERM:case DEVICECLOSE:return 1;
 case DEVICEGETNUMSUBSYSTEMS:case DEVICEGETNUMVIDEOMODES:return 1;
 case DEVICEGETCURRENTSUBSYSTEM:case DEVICEGETCURRENTVIDEOMODE:return 0;
 case DEVICESETSUBSYSTEM:case DEVICESETVIDEOMODE:return n==0;
 case DEVICEGETSUBSSYSTEMINFO:strcpy(((SubSystemInfo*)p)->name,"Nano resident GLES1");return n==0;
 case DEVICEGETVIDEOMODEINFO:{VideoMode *v=(VideoMode*)p;v->width=width;v->height=height;v->depth=16;v->flags=0;return n==0;}
 default:return 0;
 }
}
Device renderdevice={0,1,begin,end,clear,show,blit,setState,getState,
 im2DRenderLine,im2DRenderTriangle,im2DRenderPrimitive,im2DRenderIndexedPrimitive,
 im3DTransform,im3DRenderPrimitive,im3DRenderIndexedPrimitive,im3DEnd,system};
} }
