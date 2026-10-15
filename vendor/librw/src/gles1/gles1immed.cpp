#include "gles1impl.h"
namespace rw { namespace gles1 {
static Im3DVertex *verts3;static int32 count3;static uint32 flags3;
GLenum primitive(PrimitiveType p){static const GLenum map[]={GL_POINTS,GL_LINES,0x3,GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN,GL_POINTS};return (unsigned)p<7?map[p]:GL_TRIANGLES;}
static void arrays(void *p,int stride,int uv,int color,bool hasUV,bool hasColor){
 glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 glEnableClientState(GL_VERTEX_ARRAY);glVertexPointer(3,GL_FLOAT,stride,p);
 glDisableClientState(GL_NORMAL_ARRAY);
 if(hasUV){glEnableClientState(GL_TEXTURE_COORD_ARRAY);glTexCoordPointer(2,GL_FLOAT,stride,(uint8*)p+uv);}else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 if(hasColor){glEnableClientState(GL_COLOR_ARRAY);glColorPointer(4,GL_UNSIGNED_BYTE,stride,(uint8*)p+color);}else{glDisableClientState(GL_COLOR_ARRAY);glColor4f(1,1,1,1);}
 applyStates();
}
void im3DTransform(void *v,int32 n,Matrix *m,uint32 flags){verts3=(Im3DVertex*)v;count3=n;flags3=flags;restore3D();loadWorld(m);}
static void setup3(){arrays(verts3,sizeof(Im3DVertex),offsetof(Im3DVertex,u),offsetof(Im3DVertex,r),flags3&im3d::VERTEXUV,flags3&im3d::VERTEXRGBA);}
void im3DRenderPrimitive(PrimitiveType p){if(!verts3||count3<=0)return;setup3();glDrawArrays(primitive(p),0,count3);}
void im3DRenderIndexedPrimitive(PrimitiveType p,void *ix,int32 n){if(!verts3||n<=0||!ix)return;for(int32 i=0;i<n;i++)if(((uint16*)ix)[i]>=count3){unsupportedStates++;return;}setup3();glDrawElements(primitive(p),n,GL_UNSIGNED_SHORT,ix);}
void im3DEnd(){verts3=nil;count3=0;}
// Match GL3's Im2D shader: multiply clip XYZ by camera Z before GL divides.
static Im2DVertex clip2[1024];
static bool setup2(void *v,int32 n){
 if(n<=0 || n>1024){unsupportedStates++;return false;}
 Im2DVertex *src=(Im2DVertex*)v;
 for(int32 i=0;i<n;i++){
  clip2[i]=src[i];float w=src[i].w;
  clip2[i].x=(src[i].x*2/width-1)*w;
  clip2[i].y=(1-src[i].y*2/height)*w;
  clip2[i].z=src[i].z*w;
 }
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 arrays(clip2,sizeof(Im2DVertex),offsetof(Im2DVertex,u),offsetof(Im2DVertex,r),true,true);
 glVertexPointer(4,GL_FLOAT,sizeof(Im2DVertex),clip2);
 return true;
}
void im2DRenderPrimitive(PrimitiveType p,void *v,int32 n){if(!setup2(v,n))return;glDrawArrays(primitive(p),0,n);restore3D();}
void im2DRenderIndexedPrimitive(PrimitiveType p,void *v,int32 n,void *ix,int32 ni){
 if(ni<=0 || !ix || !v)return;
 for(int32 i=0;i<ni;i++)if(((uint16*)ix)[i]>=n){unsupportedStates++;return;}
 if(!setup2(v,n))return;glDrawElements(primitive(p),ni,GL_UNSIGNED_SHORT,ix);restore3D();
}
void im2DRenderLine(void *v,int32 n,int32 a,int32 b){if(a<0||b<0||a>=n||b>=n)return;uint16 ix[]={(uint16)a,(uint16)b};im2DRenderIndexedPrimitive(PRIMTYPELINELIST,v,n,ix,2);}
void im2DRenderTriangle(void *v,int32 n,int32 a,int32 b,int32 c){if(a<0||b<0||c<0||a>=n||b>=n||c>=n)return;uint16 ix[]={(uint16)a,(uint16)b,(uint16)c};im2DRenderIndexedPrimitive(PRIMTYPETRILIST,v,n,ix,3);}
} }
