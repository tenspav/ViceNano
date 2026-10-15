/* Static atomic pipeline: cached interleaved CPU vertices, native ushort indices.
 * No shaders, FBOs, VAOs, skinning or driver-side index expansion. */
#include "gles1impl.h"
#include <string.h>
namespace rw { namespace gles1 {
struct MeshInstance { Im3DVertex *vertices; bool alpha; };
struct Header : rw::InstanceDataHeader { uint16 serial;uint16 count;MeshInstance meshes[1]; };
void freeInstanceData(Geometry *g){
 if(!g->instData || g->instData->platform!=PLATFORM_GLES1)return;
 Header *h=(Header*)g->instData;
 for(uint32 i=0;i<h->count;i++)if(h->meshes[i].vertices)rwFree(h->meshes[i].vertices);
 rwFree(h);g->instData=nil;
}
static void uninstance(ObjPipeline*,Atomic *a){freeInstanceData(a->geometry);}
static void instance(ObjPipeline*,Atomic *a){
 Geometry *g=a->geometry;
 if((g->flags&Geometry::NATIVE)||g->numVertices<1||g->numVertices>65535||!g->morphTargets[0].vertices){unsupportedStates++;return;}
 if(!g->meshHeader)g->buildMeshes();
 if(g->instData){Header *h=(Header*)g->instData;if(h->platform==PLATFORM_GLES1 && h->serial==g->meshHeader->serialNum && !g->lockedSinceInst)return;freeInstanceData(g);}
 unsigned count=g->meshHeader->numMeshes;if(count<1||count>64){unsupportedStates++;return;}
 Header *h=(Header*)rwMalloc(sizeof(Header)+(count-1)*sizeof(MeshInstance),MEMDUR_EVENT);
 if(!h)return;memset(h,0,sizeof(Header)+(count-1)*sizeof(MeshInstance));h->platform=PLATFORM_GLES1;h->serial=g->meshHeader->serialNum;h->count=count;g->instData=h;
 Im3DVertex *v=(Im3DVertex*)rwMalloc(g->numVertices*sizeof(Im3DVertex),MEMDUR_FUNCTION);
 if(!v){freeInstanceData(g);return;}
 Mesh *mesh=g->meshHeader->getMeshes();
 for(unsigned j=0;j<count;j++){
  RGBA mat=mesh[j].material->color;
  for(int i=0;i<g->numVertices;i++){
   memset(&v[i],0,sizeof(v[i]));v[i].position=g->morphTargets[0].vertices[i];
   if(g->morphTargets[0].normals)v[i].normal=g->morphTargets[0].normals[i];
   RGBA c=g->colors?g->colors[i]:makeRGBA(255,255,255,255);
   v[i].setColor(c.red*mat.red/255,c.green*mat.green/255,c.blue*mat.blue/255,c.alpha*mat.alpha/255);
   if(v[i].a<255)h->meshes[j].alpha=true;
   if(g->texCoords[0]){v[i].u=g->texCoords[0][i].u;v[i].v=g->texCoords[0][i].v;}
  }
  h->meshes[j].vertices=(Im3DVertex*)rwMalloc(g->numVertices*sizeof(Im3DVertex),MEMDUR_EVENT);
  if(!h->meshes[j].vertices){rwFree(v);freeInstanceData(g);return;}
  memcpy(h->meshes[j].vertices,v,g->numVertices*sizeof(Im3DVertex));
 }
 glBindBuffer(GL_ARRAY_BUFFER,0);rwFree(v);g->lockedSinceInst=0;
}
static void render(ObjPipeline *p,Atomic *a){
 instance(p,a);Geometry *g=a->geometry;if(!g->instData)return;
 Header *h=(Header*)g->instData;Mesh *meshes=g->meshHeader->getMeshes();
 restore3D();loadWorld(a->getFrame()->getLTM());
 uintptr savedTex=(uintptr)GetRenderStatePtr(TEXTURERASTER);uint32 savedAlpha=GetRenderState(VERTEXALPHA);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
 for(unsigned j=0;j<h->count;j++){
  Mesh *mesh=&meshes[j];bool valid=true;for(unsigned i=0;i<mesh->numIndices;i++)if(mesh->indices[i]>=g->numVertices)valid=false;
  if(!valid){unsupportedStates++;continue;}
  Texture *t=mesh->material->texture;Raster *r=t?t->raster:nil;
  SetRenderStatePtr(TEXTURERASTER,r);SetRenderState(VERTEXALPHA,h->meshes[j].alpha || (r && r->format==Raster::C4444));
  if(t){SetRenderState(TEXTUREFILTER,t->getFilter());SetRenderState(TEXTUREADDRESSU,t->getAddressU());SetRenderState(TEXTUREADDRESSV,t->getAddressV());}
  applyStates();glBindBuffer(GL_ARRAY_BUFFER,0);Im3DVertex *v=h->meshes[j].vertices;
  glEnableClientState(GL_VERTEX_ARRAY);glVertexPointer(3,GL_FLOAT,sizeof(Im3DVertex),v);
  glEnableClientState(GL_COLOR_ARRAY);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Im3DVertex),(uint8*)v+offsetof(Im3DVertex,r));
  glDisableClientState(GL_NORMAL_ARRAY);
  if(r){glEnableClientState(GL_TEXTURE_COORD_ARRAY);glTexCoordPointer(2,GL_FLOAT,sizeof(Im3DVertex),(uint8*)v+offsetof(Im3DVertex,u));}else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDrawElements(g->meshHeader->flags&MeshHeader::TRISTRIP?GL_TRIANGLE_STRIP:GL_TRIANGLES,mesh->numIndices,GL_UNSIGNED_SHORT,mesh->indices);
 }
 glBindBuffer(GL_ARRAY_BUFFER,0);SetRenderStatePtr(TEXTURERASTER,(void*)savedTex);SetRenderState(VERTEXALPHA,savedAlpha);
}
ObjPipeline *makeDefaultPipeline(){ObjPipeline *p=ObjPipeline::create();p->platform=PLATFORM_GLES1;p->impl.instance=instance;p->impl.uninstance=uninstance;p->impl.render=render;return p;}
} }
