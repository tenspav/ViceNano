/* Emulator-only MBX transport. Packet layout follows nano7_mbxr.c's
 * decoded RetailOS stream; this is not a hardware-qualified MBX driver. */
#include "nanoos_shim.h"
extern "C" {
#include "lcd.h"
#include <timer.h>
#include "mbx.h"
}
#define CORE(o) (*(volatile uint32_t *)(0x3b000000u+(o)))
static uint32_t commands[64];
static unsigned commandCount;
#define FIFO commands[commandCount++]
M8GpuStatus m8_gpu_status;
// Claude's mbx_init maps DRAM. This target never overlaps scanout.
alignas(4096) static uint32_t target[240*432];
static const unsigned offsets[]={0x1000,0x1004,0x1008,0x100c,0x1010,0x1014,0x1018,0x101c,0x654,0x658,0x65c,0x660};
static uint32_t saved[12], savedControl;
alignas(4096) static uint16_t texels[32][2048];
static uint32_t textureMask, textureState[32];
static unsigned morton(unsigned x,unsigned y,unsigned lw,unsigned lh){
 unsigned out=0,shift=0,b=0;
 for(;b<lw && b<lh;b++){out|=((y>>b)&1)<<shift++;out|=((x>>b)&1)<<shift++;}
 for(;b<lw || b<lh;b++)out|=((lw>lh?x:y)>>b&1)<<shift++;
 return out;
}
static bool owned, scene;
static unsigned submitted;
int m8_gpu_texture_create(unsigned w,unsigned h,unsigned format,const uint16_t *src){
 if(!owned||scene||!src||w<8||h<8||w>32||h>32||(w&(w-1))||(h&(h-1))||(format!=1&&format!=2))return -1;
 unsigned id=0,lw=0,lh=0;
 while(id<32 && (textureMask&(1u<<id)))++id;
 if(id==32)return -1;
 for(unsigned n=w;n>1;n>>=1)++lw;
 for(unsigned n=h;n>1;n>>=1)++lh;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)texels[id][morton(x,y,lw,lh)]=src[y*w+x];
 textureState[id]=((lw-3)<<24)|((lh-3)<<20)|((format==1?5u:9u)<<12)|0x600;
 textureMask|=1u<<id;
 __asm__ volatile("dsb sy" ::: "memory");
 return id+1;
}
void m8_gpu_texture_destroy(int id){if(id>0&&id<=32)textureMask&=~(1u<<(id-1));}

static uint32_t bits(float f) { uint32_t u; __builtin_memcpy(&u,&f,4); return u; }
static void barrier(){ __asm__ volatile("dsb sy" ::: "memory"); }
static int fail(){ ++m8_gpu_status.errors;scene=false;return -1; }
int m8_gpu_init(void) {
 if(owned)return 0;
 if(CORE(0xff0)!=0xb105f00du)return fail();
 for(unsigned i=0;i<12;i++)saved[i]=CORE(offsets[i]);
 savedControl=CORE(0x1020);
 if(!mbx_init())return fail();
 textureMask=0;commandCount=0;
 owned=true;scene=false;return 0;
}
int m8_gpu_triangle(const M8GpuVertex v[3]) {
 if(!owned||!scene||submitted>=2048)return fail();
 // Dirty ISP/TSP, alpha, both texture layers, region, format. Packed
 // color slot 2 and xyz slot 0 (0x503); disable textures by omitting uv.
 FIFO=0x1000003fu;
 FIFO=(1u<<29)|(1u<<22)|(7u<<9); // ONE,ZERO blend; Gouraud; always
 FIFO=0; // alpha test (pass-through path bypasses it)
 for(unsigned i=0;i<6;i++)FIFO=0; // both texture layers
 FIFO=0; // region
 FIFO=0x503;
 FIFO=0x70000000;FIFO=0xb00b0800; // triangle list, pass-through
 for(unsigned i=0;i<3;i++) {
  FIFO=bits(239.f-v[i].y);FIFO=bits(v[i].x);FIFO=bits(v[i].z);
  uint32_t c=v[i].argb;
  FIFO=(c&0xff00ff00u)|((c>>16)&255)|((c&255)<<16);
 }
 mbx_submit(commands,commandCount);commandCount=0;
 ++submitted;++m8_gpu_status.triangles;return 0;
}
int m8_gpu_textured_triangle(int texture,const M8GpuTexVertex v[3]){
 if(!owned||!scene||submitted>=2048||texture<1||texture>32||!(textureMask&(1u<<(texture-1))))return fail();
 unsigned id=texture-1;
 FIFO=0x1000003f;
 FIFO=(1u<<29)|(1u<<22)|(1u<<17)|(7u<<9);
 FIFO=0;
 FIFO=textureState[id];FIFO=MBX_VA(texels[id])>>7;FIFO=0x86084610; // MODULATE
 for(unsigned i=0;i<3;i++)FIFO=0;
 FIFO=0;FIFO=0x2503; // xyz, packed color, uv
 FIFO=0x70000000;FIFO=0xb00b0800;
 for(unsigned i=0;i<3;i++){
  FIFO=bits(239.f-v[i].p.y);FIFO=bits(v[i].p.x);FIFO=bits(v[i].p.z);
  uint32_t c=v[i].p.argb;
  FIFO=(c&0xff00ff00u)|((c>>16)&255)|((c&255)<<16);
  FIFO=bits(v[i].u);FIFO=bits(v[i].v);
 }
 mbx_submit(commands,commandCount);commandCount=0;++submitted;++m8_gpu_status.triangles;return 0;
}
int m8_gpu_begin(uint32_t clear) {
 if(!owned||scene)return fail();
 mbx_begin();scene=true;submitted=0;commandCount=0;
 // An oversize screen triangle clears through the GPU, including corners.
 // Coordinates rotate to native panel space in m8_gpu_triangle.
 const M8GpuVertex v[3]={{0,240,1,clear},{0,-240,1,clear},{864,240,1,clear}};
 return m8_gpu_triangle(v);
}
int m8_gpu_end(void) {
 if(!owned||!scene)return fail();
 int r=mbx_render(target,240,432,240);
 // mbx_render currently kicks even after a TA timeout; drain its job before
 // releasing any texture/target state, and report either failure.
 int done=mbx_wait_done(20000);
 m8_gpu_status.last_status=mbx.status;
 scene=false;
 if(r<0||done<0){++m8_gpu_status.timeouts;return fail();}
 barrier();
 for(int x=0;x<432;x++)for(int y=0;y<240;y++)
  lcd_pixel(x,y,target[x*240+239-y]);
 ++m8_gpu_status.frames;return 0;
}
void m8_gpu_shutdown(void) {
 if(!owned)return;
 // Submission is synchronous, so no queued job may still own target.
 // On timeout the backend is unusable until the next emulator boot.
 for(unsigned i=0;i<12;i++)CORE(offsets[i])=saved[i];
 CORE(0x1020)=savedControl|0x100;
 barrier();owned=scene=false;
}
