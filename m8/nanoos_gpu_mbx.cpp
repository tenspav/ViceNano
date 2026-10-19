/* Emulator-only MBX transport. Packet layout follows nano7_mbxr.c's
 * decoded RetailOS stream; this is not a hardware-qualified MBX driver. */
#include "nanoos_shim.h"
extern "C" {
#include "lcd.h"
#include <timer.h>
#include "mbx.h"
#include "memmap.h"
void input_service(void); // nanoos/src/input.h (m8 has its own input.h)
extern unsigned input_i2c_n; // nanoos/src/input.c, PMU reads issued
}
#define CORE(o) (*(volatile uint32_t *)(0x3b000000u+(o)))
static uint32_t commands[64];
static unsigned commandCount;
#define FIFO commands[commandCount++]
M8GpuStatus m8_gpu_status;
// Claude's mbx_init maps DRAM. This target never overlaps scanout.
// GPU-written, CPU-read: the uncached kernel .dma (nanoos memmap.h)
alignas(4096) static uint32_t target[240*432] MM_DMA_BUF;
static const unsigned offsets[]={0x1000,0x1004,0x1008,0x100c,0x1010,0x1014,0x1018,0x101c,0x654,0x658,0x65c,0x660};
static uint32_t saved[12], savedControl;
alignas(4096) static uint16_t texels[32][2048] MM_DMA_BUF;
static uint32_t textureMask, textureState[32];
// State words as RetailOS's pass-through clears send them (gltest trace,
// header 0x10000073: ISP 0x22200e00, alpha 0xe0000000, region 0x2003801a,
// block 6 = 0). Alpha word 0 is the driver's never-pass encoding: NanoEmu
// forces "always" for pass-through draws, hardware rejects every fragment.
// ISP bits 25 and 21 are set in every traced RetailOS ISP word; unknown role.
#define ISP_RETAIL ((1u<<25)|(1u<<21))
#define ALPHA_ALWAYS 0xe0000000u
#define REGION_RETAIL 0x2003801au
// Round 9: one textured-triangle packet per texture, CPU RAM (not the GPU
// PB). Its 14 header words are immutable per texture and built once at
// texture_create (scene load); m8_gpu_textured_triangle writes only the 18
// vertex words (xyz, packed fog color, uv) and submits it as before.
#define TEX_HDR 14u
static uint32_t texPacket[32][TEX_HDR+18];
static unsigned morton(unsigned x,unsigned y,unsigned lw,unsigned lh){
 unsigned out=0,shift=0,b=0;
 for(;b<lw && b<lh;b++){out|=((y>>b)&1)<<shift++;out|=((x>>b)&1)<<shift++;}
 for(;b<lw || b<lh;b++)out|=((lw>lh?x:y)>>b&1)<<shift++;
 return out;
}
static bool owned, scene;
static unsigned submitted, recoveries;
#define MAX_RECOVERIES 8u
int m8_gpu_texture_create(unsigned w,unsigned h,unsigned format,const uint16_t *src){
 if(!owned||scene||!src||w<8||h<8||w>32||h>32||(w&(w-1))||(h&(h-1))||(format!=1&&format!=2))return -1;
 unsigned id=0,lw=0,lh=0;
 while(id<32 && (textureMask&(1u<<id)))++id;
 if(id==32)return -1;
 for(unsigned n=w;n>1;n>>=1)++lw;
 for(unsigned n=h;n>1;n>>=1)++lh;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)texels[id][morton(x,y,lw,lh)]=src[y*w+x];
 textureState[id]=((lw-3)<<24)|((lh-3)<<20)|((format==1?5u:9u)<<12)|0x600;
 const uint32_t hdr[TEX_HDR]={0x1000007f,(1u<<29)|(1u<<22)|(1u<<17)|(7u<<9)|ISP_RETAIL,ALPHA_ALWAYS,
  textureState[id],MBX_VA(texels[id])>>7,0x86084610, // MODULATE
  0,0,0,REGION_RETAIL,0x2503,0, // xyz, packed color, uv
  0x70000000,0xb00b0800};
 for(unsigned i=0;i<TEX_HDR;i++)texPacket[id][i]=hdr[i];
 textureMask|=1u<<id;
 __asm__ volatile("dsb sy" ::: "memory");
 return id+1;
}
void m8_gpu_texture_destroy(int id){if(id>0&&id<=32)textureMask&=~(1u<<(id-1));}

static uint32_t bits(float f) { uint32_t u; __builtin_memcpy(&u,&f,4); return u; }
static void barrier(){ __asm__ volatile("dsb sy" ::: "memory"); }
static int fail(){ ++m8_gpu_status.errors;scene=false;return -1; }
// A world frame is 100+ ms of CPU and GPU waits: sample the buttons inside
// it so Home is not missed (NanoOS sampled them once per frame). Every 16th
// triangle only, so triangle (2) and quad (3) scenes are untouched.
// Round 15: off by default (M8_SCENE_POLL, nanoos_shim.h), only counted.
static void keys_every16(){
 if(submitted&15)return;
#if M8_SCENE_POLL
 ++mbx_probe.svc_now;input_service(); // svc: round-12 probe
#else
 ++m8_i2c.skip;
#endif
}
// Round 15: close the scene's I2C window (job done or failed)
static void i2c_scene_end(bool failed){
 M8I2cWatch &w=m8_i2c;
 w.mid=input_i2c_n-w.i2c_b;
 w.total_mid+=w.mid;if(w.mid>w.max_mid)w.max_mid=w.mid;
 w.ring[w.scenes++%8]=w.mid;
 if(failed&&!w.fail_scene){w.fail_scene=mbx_fail.scene;w.fail_mid=w.mid;w.fail_skip=w.skip;}
}
int m8_gpu_init(void) {
 m8_gpu_diag.called=1;
 m8_gpu_diag.target=(uint32_t)(uintptr_t)target;
 if(owned){m8_gpu_diag.reason=0;return 0;}
 // The nano 7G core has no 0xb105f00d signature (+0xff0 reads 0x00010001) and
 // may be power-gated until mbx_init() runs, so let the driver identify it.
 int r=mbx_init();
 m8_gpu_diag.init_ret=(uint32_t)r;
 m8_gpu_diag.id=mbx.id;m8_gpu_diag.rev=mbx.rev;m8_gpu_diag.sig=mbx.sig;
 if(!r){m8_gpu_diag.reason=mbx.id?"mbx_init failed (MMU/status)":"core ID invalid";return fail();}
 m8_gpu_diag.reason=0;
 recoveries=0;
 for(unsigned i=0;i<12;i++)saved[i]=CORE(offsets[i]);
 savedControl=CORE(0x1020);
 textureMask=0;commandCount=0;
 owned=true;scene=false;return 0;
}
int m8_gpu_triangle(const M8GpuVertex v[3]) {
 if(!owned||!scene||submitted>=2048)return fail();
 // Dirty ISP/TSP, alpha, both texture layers, region, format, block 6.
 // Packed color slot 2 and xyz slot 0 (0x503); disable textures by omitting uv.
 FIFO=0x1000007fu;
 FIFO=(1u<<29)|(1u<<22)|(7u<<9)|ISP_RETAIL; // ONE,ZERO blend; Gouraud; always
 FIFO=ALPHA_ALWAYS;
 for(unsigned i=0;i<6;i++)FIFO=0; // both texture layers
 FIFO=REGION_RETAIL;
 FIFO=0x503;
 FIFO=0;
 FIFO=0x70000000;FIFO=0xb00b0800; // triangle list, pass-through
 for(unsigned i=0;i<3;i++) {
  FIFO=bits(239.f-v[i].y);FIFO=bits(v[i].x);FIFO=bits(v[i].z);
  uint32_t c=v[i].argb;
  FIFO=(c&0xff00ff00u)|((c>>16)&255)|((c&255)<<16);
 }
 mbx_submit(commands,commandCount);commandCount=0;
 ++submitted;++m8_gpu_status.triangles;keys_every16();return 0;
}
int m8_gpu_textured_triangle(int texture,const M8GpuTexVertex v[3]){
 if(!owned||!scene||submitted>=2048||texture<1||texture>32||!(textureMask&(1u<<(texture-1))))return fail();
 uint32_t *pk=texPacket[texture-1],*w=pk+TEX_HDR;
 for(unsigned i=0;i<3;i++){
  *w++=bits(239.f-v[i].p.y);*w++=bits(v[i].p.x);*w++=bits(v[i].p.z);
  uint32_t c=v[i].p.argb;
  *w++=(c&0xff00ff00u)|((c>>16)&255)|((c&255)<<16);
  *w++=bits(v[i].u);*w++=bits(v[i].v);
 }
 mbx_submit(pk,TEX_HDR+18);++submitted;++m8_gpu_status.triangles;keys_every16();return 0;
}
int m8_gpu_begin(uint32_t clear) {
 if(!owned||scene)return fail();
 mbx_begin();scene=true;submitted=0;commandCount=0;
 m8_i2c.i2c_b=input_i2c_n;m8_i2c.skip=0; // round 15: scene I2C window opens
 // An oversize screen triangle clears through the GPU, including corners.
 // Coordinates rotate to native panel space in m8_gpu_triangle.
 const M8GpuVertex v[3]={{0,240,1,clear},{0,-240,1,clear},{864,240,1,clear}};
 return m8_gpu_triangle(v);
}
// Bring-up probes: the CPU pre-fills the target with SENTINEL and the ISP
// background (+0x63c) is BACKGROUND, so the copied frame tells which stage
// wrote each pixel. Both are symmetric under an R/B swap.
#define SENTINEL 0xff00ffffu   // cyan: the ISP never wrote this pixel
#define BACKGROUND 0xffff00ffu // magenta: ISP ran, no object covered it
// m8_gpu_direct (mode 4): render straight into the LCD back buffer, which
// has the target's layout and format. The sentinel fill and the copy below
// are ~300k uncached accesses per frame (D-cache off); the pixel classes
// then stay 0. The clear triangle covers every pixel.
int m8_gpu_direct;
int m8_gpu_end(void) {
 if(!owned||!scene)return fail();
 M8GpuDiag &d=m8_gpu_diag;
 uint32_t *dst=m8_gpu_direct?lcd_back():target;
 if(!m8_gpu_direct)for(unsigned i=0;i<240*432;i++)target[i]=SENTINEL;
 barrier();
 mbx_set_background(BACKGROUND);
 uint32_t scenes=mbx.scenes,jobs=mbx.jobs,t0=USEC;
 d.stage="TA end";
 int r=mbx_render(dst,240,432,240);
 // mbx_render currently kicks even after a TA timeout; drain its job before
 // releasing any texture/target state, and report either failure.
 int done=mbx_wait_done(20000);
 i2c_scene_end(r<0||done<0); // before the first between-scene sampling
 input_service(); // the job is over: no MMIO timing changes
 d.render_us=USEC-t0;
 d.frames++;d.sess_frames++;d.segments=submitted;
 d.ta_ok=mbx.scenes!=scenes;d.isp_ok=mbx.jobs!=jobs;
 d.ta_us=mbx.done_us;d.isp_us=mbx.job_us;
 d.ta_status=mbx.ta_status;d.isp_status=mbx.job_status;d.timeouts=mbx.timeouts;
 d.fb_va=MBX_VA(dst);d.fmt=CORE(0x650);d.stride=CORE(0x660);
 d.pb_used=mbx_pb_used();
 d.stage=!d.ta_ok?"TA (no done bit 4)":!d.isp_ok?"ISP (no done 0x44)":"done";
 m8_gpu_status.last_status=mbx.status;
 scene=false;
 if(r<0||done<0){++m8_gpu_status.timeouts;return fail();}
 barrier();
 uint32_t gpu=0,bg=0,untouched=0,black=0;
 if(m8_gpu_direct){d.px_gpu=d.px_bg=d.px_untouched=d.px_black=0;d.center=dst[216*240+239-120];++m8_gpu_status.frames;return 0;}
 for(int x=0;x<432;x++){if(!(x&63))input_service();for(int y=0;y<240;y++){
  uint32_t c=target[x*240+239-y];
  if(c==SENTINEL)++untouched;else if((c&0xffffff)==(BACKGROUND&0xffffff))++bg;
  else{++gpu;black+=!(c&0xffffff);}
  lcd_pixel(x,y,c);
 }}
 d.px_gpu=gpu;d.px_bg=bg;d.px_untouched=untouched;d.px_black=black;
 static uint32_t rearms;
 if(mbx_pw.rearms!=rearms){rearms=mbx_pw.rearms;++d.rearm_frames;d.rearm_bad+=gpu<240*432/2||black>240*432/2;}
 d.center=target[216*240+239-120];
 ++m8_gpu_status.frames;return 0;
}
int m8_gpu_recover(void) {
 if(!owned||recoveries>=MAX_RECOVERIES)return -1;
 ++recoveries;++m8_gpu_diag.recoveries;scene=false;commandCount=0;
 int r=mbx_init();
 if(!r){m8_gpu_diag.reason="re-init failed";return fail();}
 return 0;
}
void m8_gpu_shutdown(void) {
 if(!owned)return;
 // Submission is synchronous, so no queued job may still own target.
 // On timeout the backend is unusable until the next emulator boot.
 for(unsigned i=0;i<12;i++)CORE(offsets[i])=saved[i];
 CORE(0x1020)=savedControl|0x100;
 barrier();owned=scene=false;
}
