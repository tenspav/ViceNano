#include "../vendor/librw/rw.h"
#include "model.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <stdexcept>
using namespace rw;
struct Tex {std::string name;int w,h,fmt;std::vector<uint16> pixels;};
struct Part {int tex;std::vector<NanVertex> vertices;std::vector<uint16> indices;};
static void require(bool b,const char *why){if(!b)throw std::runtime_error(why);}
static int pot(int n){int p=1;while(p<n&&p<64)p*=2;return p;}
int main(int argc,char **argv){try{
 require(argc==4,"usage: converter MODEL.dff TEXTURES.txd OUTPUT.h");
 require(Engine::init(),"init");registerMeshPlugin();registerNativeDataPlugin();
 require(Engine::open(nil)&&Engine::start(),"start");
 StreamFile ts;require(ts.open(argv[2],"rb")&&findChunk(&ts,ID_TEXDICTIONARY,nil,nil),"TXD header");
 TexDictionary *txd=TexDictionary::streamRead(&ts);ts.close();require(txd!=nil,"TXD read");TexDictionary::setCurrent(txd);
 StreamFile ds;require(ds.open(argv[1],"rb")&&findChunk(&ds,ID_CLUMP,nil,nil),"DFF header");Clump *clump=Clump::streamRead(&ds);ds.close();require(clump!=nil,"DFF read");
 std::vector<Tex> textures;std::vector<Part> parts;std::map<std::string,int> ids;
 float lo[3]={1e20f,1e20f,1e20f},hi[3]={-1e20f,-1e20f,-1e20f};
 FORLIST(link,clump->atomics){
  Atomic *a=Atomic::fromClump(link);Geometry *g=a->geometry;
  require(!(g->flags&Geometry::NATIVE)&&g->numVertices>0&&g->numVertices<65536,"unsupported geometry");
  for(int material=0;material<g->matList.numMaterials;material++){
   Part p;p.tex=-1;Material *mat=g->matList.materials[material];Texture *t=mat->texture;
   if(t){
    std::string name=t->name;
    if(!ids.count(name)){
     require(t->raster!=nil,"missing texture raster");Image *im=t->raster->toImage();require(im!=nil,"texture decode");
     if(im->depth<=8)im->unpalettize();if(im->depth!=32)im->convertTo32();require(im->depth==32,"RGBA conversion");
     bool alpha=false;for(int y=0;y<im->height;y++)for(int x=0;x<im->width;x++)if(im->pixels[y*im->stride+4*x+3]<255)alpha=true;
     Tex tex;tex.name=name;tex.w=pot(im->width);tex.h=pot(im->height);tex.fmt=alpha?2:1;
     for(int y=0;y<tex.h;y++)for(int x=0;x<tex.w;x++){
      uint8 *c=im->pixels+(y*im->height/tex.h)*im->stride+(x*im->width/tex.w)*4;
      tex.pixels.push_back(alpha?((c[0]>>4)<<12)|((c[1]>>4)<<8)|((c[2]>>4)<<4)|(c[3]>>4):((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3));
     }
     im->destroy();ids[name]=textures.size();textures.push_back(tex);
    }p.tex=ids[name];
   }
   for(int i=0;i<g->numVertices;i++){
    V3d src=g->morphTargets[0].vertices[i],v;
    V3d::transformPoints(&v,&src,1,a->getFrame()->getLTM());
    NanVertex n={v.x,v.z,-v.y,0,0,255,255,255,255};
    if(g->texCoords[0]){n.u=g->texCoords[0][i].u;n.v=g->texCoords[0][i].v;}
    RGBA c=g->colors?g->colors[i]:makeRGBA(255,255,255,255);
    n.r=c.red*mat->color.red/255;n.g=c.green*mat->color.green/255;n.b=c.blue*mat->color.blue/255;n.a=c.alpha*mat->color.alpha/255;
    p.vertices.push_back(n);
   }
   for(int i=0;i<g->numTriangles;i++)if(g->triangles[i].matId==material)for(int j=0;j<3;j++){
    unsigned ix=g->triangles[i].v[j];require(ix<p.vertices.size(),"index out of range");p.indices.push_back(ix);
    NanVertex &v=p.vertices[ix];float xyz[]={v.x,v.y,v.z};for(int k=0;k<3;k++){lo[k]=fminf(lo[k],xyz[k]);hi[k]=fmaxf(hi[k],xyz[k]);}
   }
   if(!p.indices.empty()){
    // Keep only vertices used by this material; the source geometry is shared.
    std::vector<NanVertex> compact;std::map<uint16,uint16> remap;
    for(auto &ix:p.indices){auto found=remap.find(ix);if(found==remap.end()){uint16 next=compact.size();compact.push_back(p.vertices[ix]);remap[ix]=next;ix=next;}else ix=found->second;}
    p.vertices.swap(compact);parts.push_back(p);
   }
  }
 }
 require(!parts.empty()&&!textures.empty(),"model must have textured triangles");
 float extent=fmaxf(hi[0]-lo[0],fmaxf(hi[1]-lo[1],hi[2]-lo[2]));require(extent>0,"empty bounds");
 for(auto &p:parts)for(auto &v:p.vertices){v.x=(v.x-(lo[0]+hi[0])*.5f)*2.6f/extent;v.y=(v.y-(lo[1]+hi[1])*.5f)*2.6f/extent;v.z=(v.z-(lo[2]+hi[2])*.5f)*2.6f/extent;}
 std::ofstream out(argv[3]);require(bool(out),"output open");out<<"// Generated locally from user-owned Vice City assets; do not distribute.\n#pragma once\n#include \"model.h\"\n"<<std::scientific<<std::setprecision(9);
 size_t vertexBytes=0,indexBytes=0,textureBytes=0;
 for(size_t i=0;i<textures.size();i++){auto &t=textures[i];out<<"static const uint16_t tex"<<i<<"[]={";for(auto p:t.pixels)out<<p<<",";out<<"};\n";textureBytes+=t.pixels.size()*2;}
 for(size_t i=0;i<parts.size();i++){
  auto &p=parts[i];out<<"static const NanVertex verts"<<i<<"[]={\n";for(auto &v:p.vertices)out<<"{"<<v.x<<"f,"<<v.y<<"f,"<<v.z<<"f,"<<v.u<<"f,"<<v.v<<"f,"<<(int)v.r<<","<<(int)v.g<<","<<(int)v.b<<","<<(int)v.a<<"},\n";out<<"};\nstatic const uint16_t ix"<<i<<"[]={";for(auto x:p.indices)out<<x<<",";out<<"};\n";vertexBytes+=p.vertices.size()*sizeof(NanVertex);indexBytes+=p.indices.size()*2;
 }
 out<<"static const NanTexture modelTextures[]={\n";for(size_t i=0;i<textures.size();i++){auto &t=textures[i];out<<"{"<<t.w<<","<<t.h<<","<<t.fmt<<",tex"<<i<<",\""<<t.name<<"\"},\n";}out<<"};\nstatic const NanPart modelParts[]={\n";
 for(size_t i=0;i<parts.size();i++){auto &p=parts[i];out<<"{"<<p.vertices.size()<<","<<p.indices.size()/3<<","<<p.tex<<",verts"<<i<<",ix"<<i<<"},\n";}out<<"};\n";out.close();
 std::cout<<"parts="<<parts.size()<<" textures="<<textures.size()<<" vertices_bytes="<<vertexBytes<<" index_bytes="<<indexBytes<<" texture_bytes="<<textureBytes<<"\n";
 for(auto &t:textures)std::cout<<t.name<<" "<<t.w<<"x"<<t.h<<" "<<(t.fmt==1?"RGB565":"RGBA4444")<<"\n";
 clump->destroy();txd->destroy();Engine::stop();Engine::close();Engine::term();return 0;
}catch(const std::exception &e){std::cerr<<"converter: "<<e.what()<<"\n";return 1;}}
