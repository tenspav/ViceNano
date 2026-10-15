/* M2 bounded 16-bit base-level raster path; M3 adds converted asset pipelines. */
#include "gles1impl.h"
#include <string.h>
namespace rw { namespace gles1 {
int32 nativeRasterOffset;
NativeRaster *nativeRaster(Raster *r){return PLUGINOFFSET(NativeRaster,r,nativeRasterOffset);}
static void *ctor(void *o,int32,int32){memset(nativeRaster((Raster*)o),0,sizeof(NativeRaster));return o;}
static void *dtor(void *o,int32,int32){NativeRaster *n=nativeRaster((Raster*)o);if(n->texture)glDeleteTextures(1,&n->texture);if(n->data)rwFree(n->data);return o;}
static Raster *create(Raster *r){
 if(r->type==Raster::CAMERA || r->type==Raster::ZBUFFER || (r->flags&Raster::DONTALLOCATE))return r;
 if(r->type!=Raster::TEXTURE || r->width<1 || r->height<1 || r->width>512 || r->height>512 ||
 (r->width&(r->width-1)) || (r->height&(r->height-1)) ||
 (r->format!=Raster::C565 && r->format!=Raster::C4444)){r->destroy();return nil;}
 NativeRaster *n=nativeRaster(r);n->bytes=r->width*r->height*2;
 n->data=(uint8*)rwMalloc(n->bytes,MEMDUR_EVENT);if(!n->data){r->destroy();return nil;}
 memset(n->data,0,n->bytes);r->depth=16;r->stride=r->width*2;
 glGenTextures(1,&n->texture);return r;
}
static uint8 *lock(Raster *r,int32 level,int32){return level==0?nativeRaster(r)->data:nil;}
static void unlock(Raster *r,int32 level){
 NativeRaster *n=nativeRaster(r);if(level!=0||!n->texture||!n->data)return;
 GLenum fmt=r->format==Raster::C565?GL_RGB:GL_RGBA;glBindTexture(GL_TEXTURE_2D,n->texture);
 glPixelStorei(GL_UNPACK_ALIGNMENT,2);glTexImage2D(GL_TEXTURE_2D,0,fmt,r->width,r->height,0,fmt,
 r->format==Raster::C565?GL_UNSIGNED_SHORT_5_6_5:GL_UNSIGNED_SHORT_4_4_4_4,n->data);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
}
static int32 levels(Raster*){return 1;}
static void *open(void *o,int32,int32){Driver *d=engine->driver[PLATFORM_GLES1];d->defaultPipeline=makeDefaultPipeline();d->rasterNativeOffset=nativeRasterOffset;d->rasterCreate=create;d->rasterLock=lock;d->rasterUnlock=unlock;d->rasterNumLevels=levels;return o;}
static void *close(void *o,int32,int32){return o;}
void registerPlatformPlugins(){
 nativeRasterOffset=Raster::registerPlugin(sizeof(NativeRaster),0x564e0001,ctor,dtor,nil);
 Driver::registerPlugin(PLATFORM_GLES1,0,PLATFORM_GLES1,open,close);
}
} }
