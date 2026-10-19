#include "nanoos_shim.h"
#include "nano.h"
#include "input.h"
extern "C" {
#include "lcd.h"
#include <timer.h>
#include "fmt.h"
#include "mbx.h"
#include "report.h"
void reset_stockos(void) __attribute__((noreturn)); // nanoos/src/reset.c
extern unsigned input_i2c_n; // nanoos/src/input.c (round 15)
unsigned input_held(void);   // held buttons (nanoos/src/input.h; m8 has its own input.h)
}
bool m8_walk(unsigned held,unsigned edges); // nano.cpp, mode 4 camera
bool m8_walk_home();
extern bool m8PadExit; // nano.cpp: Start/PS/Home on the BT pad
struct M8GpuDiag m8_gpu_diag;
struct M8I2cWatch m8_i2c;
void m8_diag_draw(int x, int y, uint32_t color) {
 const struct M8GpuDiag &d=m8_gpu_diag;
 const int s=14; // line pitch: one blank pixel row pair between lines
 fmt_str("GPU init called: ");fmt_str(d.called?"yes":"no");fmt_line(x,y,color);
 fmt_str("mbx_init returned ");fmt_sdec((int32_t)d.init_ret);fmt_line(x,y+s,color);
 fmt_str("core id ");fmt_hex(d.id,8);fmt_str(" rev ");fmt_hex(d.rev,8);fmt_line(x,y+2*s,color);
 fmt_str("sig ");fmt_hex(d.sig,8);fmt_str(d.id?" (core ID ok)":" (core ID zero)");fmt_line(x,y+3*s,color);
 fmt_str("target buffer addr ");fmt_hex(d.target,8);fmt_line(x,y+4*s,color);
 // mbx_init() returns 1 on success (m8_gpu_init() maps that to 0).
 if(d.called&&d.init_ret==1&&!d.reason){fmt_str("MBX available");fmt_line(x,y+5*s,color);}
 else{fmt_str("MBX unavailable: ");fmt_str(d.reason?d.reason:d.called?"mbx_init failed":"init not run");fmt_line(x,y+5*s,color);}
 m8_diag_render_draw(x,y+6*s,color);
}
// RSN 9: did the ISP job end after the 20 ms timeout (mbx.h isp_late_us)?
static void late(const struct mbx_fail_info &f){
 if(f.reason!=9)return;
 if(f.isp_late_us==MBX_NEVER){fmt_str(" (dead ");fmt_dec(MBX_ISP_LATE_US/1000);fmt_str("ms)");}
 else{fmt_str(" (late ");fmt_dec(f.isp_late_us);fmt_str("us)");}
}
int m8_diag_render_draw(int x, int y, uint32_t color) {
 const struct M8GpuDiag &d=m8_gpu_diag;
 const int s=10;
 if(!d.frames)return y;
 fmt_str("fr ");fmt_dec(d.frames);fmt_str(" seg ");fmt_dec(d.segments);fmt_str(" stage ");fmt_str(d.stage?d.stage:"-");fmt_line(x,y,color);
 fmt_str("TA ");fmt_str(d.ta_ok?"done ":"FAIL ");fmt_dec(d.ta_us);fmt_str("us st ");fmt_hex(d.ta_status,8);fmt_line(x,y+s,color);
 fmt_str("ISP ");fmt_str(d.isp_ok?"done ":"FAIL ");fmt_dec(d.isp_us);fmt_str("us st ");fmt_hex(d.isp_status,8);fmt_line(x,y+2*s,color);
 fmt_str("render ");fmt_dec(d.render_us);fmt_str("us to ");fmt_dec(d.timeouts);fmt_str(" PB used ");fmt_dec(d.pb_used);fmt_line(x,y+3*s,color);
 fmt_str("fb ");fmt_hex(d.target,8);fmt_str(" va ");fmt_hex(d.fb_va,8);fmt_str(" fmt ");fmt_hex(d.fmt,2);fmt_str(" st ");fmt_dec(d.stride);fmt_line(x,y+4*s,color);
 fmt_str("px gpu ");fmt_dec(d.px_gpu);fmt_str(" bg ");fmt_dec(d.px_bg);fmt_str(" cpu ");fmt_dec(d.px_untouched);fmt_line(x,y+5*s,color);
 fmt_str("black ");fmt_dec(d.px_black);fmt_str(" center ");fmt_hex(d.center,8);
 // rb: frames first after a re-arm / bad of those (nanoos_shim.h)
 fmt_str(" rb ");fmt_dec(d.rearm_frames);fmt_str("/");fmt_dec(d.rearm_bad);fmt_line(x,y+6*s,color);
 // last failed scene (mbx.h mbx_fail): reason, scene, raw +0x12c, re-inits
 const struct mbx_fail_info &f=mbx_fail;
 fmt_str("fail ");fmt_dec(f.fails);fmt_str(" rsn ");fmt_dec(f.reason);fmt_str(" sc ");fmt_dec(f.fscene);
 fmt_str(" st ");fmt_hex(f.st,8);fmt_str(" rec ");fmt_dec(d.recoveries);fmt_line(x,y+7*s,color);
 if(!f.fails)return y+8*s;
 fmt_str("rsn ");fmt_dec(f.reason);fmt_str(" = ");fmt_str(mbx_fail_text(f.reason));late(f);fmt_line(x,y+8*s,color);
 return y+9*s;
}
// Round 12 TA-stall probe (mbx.h mbx_tafail), last failed scene.
// Read it as: "late" = RSN 8 TA finished after the 100 ms (slow, not dead);
// "id" 0 or clk4c/6c changed b->f = core lost clock/power; the ring shows
// when +0x12c bit 0x8000 dropped (seg, us) and svc = input_service calls
// by then (an I2C gate toggle just before the drop = hypothesis 1);
// h838 = +0x838 after the last good TA waits (drift = page leak, hyp. 2).
static void ta_probe_lines(){
 const struct mbx_ta_probe &p=mbx_tafail;
 fmt_str("probe late ");if(mbx_fail.reason!=8)fmt_str("-");else if(p.late_us==MBX_NEVER)fmt_str("never");else{fmt_dec(p.late_us);fmt_str("us");}
 fmt_str(" st ");fmt_hex(p.late_st,8);fmt_str(" id ");fmt_hex(p.id,8);fmt_str(" st3 ");
 for(int i=0;i<3;i++){fmt_hex(p.st3[i],8);fmt_str(" ");}fmt_line(0,0,0);
 fmt_str("probe clk4c ");fmt_hex(p.clk4c_b,8);fmt_str(">");fmt_hex(p.clk4c_f,8);
 fmt_str(" 6c ");fmt_hex(p.clk6c_b,8);fmt_str(">");fmt_hex(p.clk6c_f,8);
 fmt_str(" gap ");fmt_dec(p.gap_max_us);fmt_str("us@seg ");fmt_dec(p.gap_seg);fmt_str(" svc ");fmt_dec(p.svc_now);fmt_line(0,0,0);
 fmt_str("probe 130 ");fmt_hex(p.r130,8);fmt_str(" 824 ");fmt_hex(p.r824,8);fmt_str(" 838 ");fmt_hex(p.r838,8);
 fmt_str(" 83c ");fmt_hex(p.r83c,8);fmt_str(" 844 ");fmt_hex(p.r844,8);fmt_line(0,0,0);
 fmt_str("probe h838 n ");fmt_dec(p.h_n);fmt_str(":");
 for(uint32_t k=p.h_n>8?p.h_n-8:0;k<p.h_n;k++){fmt_str(" ");fmt_hex(p.h838[k%8],5);}fmt_line(0,0,0);
 fmt_str("probe 12c changes ");fmt_dec(p.n);fmt_str(" (seg us st svc):");fmt_line(0,0,0);
 for(uint32_t k=p.n>MBX_TRK?p.n-MBX_TRK:0;k<p.n;k++){uint32_t i=k%MBX_TRK;
  fmt_str("  ");fmt_dec(p.seg[i]);fmt_str(" ");fmt_dec(p.us[i]);fmt_str(" ");fmt_hex(p.st[i],8);fmt_str(" ");fmt_dec(p.svc[i]);fmt_line(0,0,0);}
}
// Round 13 pool watch (mbx.h mbx_pw). "used" = pages written since the last
// allocator init; d = new pages per scene (oldest first) with +0x838 after
// it; rearm = proactive GPU re-inits before the pool could run dry. If the
// fix works: used climbs by about d per scene, never passes 64, rearm grows
// every few scenes and there is no failure. aux: links != index + 1 at the
// last re-arm (1) or failure (2); end = aux[82c page]: 82c + 1 = never
// wrapped, 828 (first page) = pages came back FIFO (no leak).
static void pool_lines(){
 const struct mbx_pool_watch &w=mbx_pw;
 fmt_str("pool13 arms ");fmt_dec(w.arms);fmt_str(" rearm ");fmt_dec(w.rearms);fmt_str(" bycount ");fmt_dec(w.rearm_n);fmt_str(" used ");fmt_dec(w.used);
 fmt_str(" n ");fmt_dec(w.n);fmt_str(" dmax ");fmt_dec(w.dmax);fmt_str(" w8 ");fmt_dec(w.w8_max);fmt_str("us miss ");fmt_dec(w.w8_miss);fmt_line(0,0,0);
 fmt_str("pool13 d/838:");
 for(uint32_t k=w.n>8?w.n-8:0;k<w.n;k++){fmt_str(" ");fmt_dec(w.d[k%8]);fmt_str("/");fmt_hex(w.r838[k%8],4);}
 fmt_str(" 820 ");fmt_hex(w.r820[(w.n+7)%8],8);fmt_line(0,0,0);
 if(!w.a_why)return;
 fmt_str("pool13 aux@");fmt_str(w.a_why==1?"rearm":"fail");fmt_str(" used ");fmt_dec(w.a_used);fmt_str(" n ");fmt_dec(w.a_n);
 fmt_str(" links ");fmt_dec(w.a_links);fmt_str(" end ");fmt_hex(w.a_end,4);
 for(uint32_t i=0;i<w.a_links&&i<4;i++){fmt_str(" ");fmt_hex(w.a_idx[i],4);fmt_str(":");fmt_hex(w.a_val[i],4);}
 fmt_line(0,0,0);
}
// Round 15 (nanoos_shim.h M8I2cWatch): PMU I2C reads inside each scene.
// scenepoll OFF + mid 0 everywhere + still RSN 8 = hypothesis 1 is dead
// (fallback build only; the default round-15 build keeps scenepoll on).
// gate: I2C1 clock gates at scene begin > failure (4c bit 6, 6c bit 12;
// 1 = gated off, the state input.c leaves after every transfer).
static void i2c_lines(){
 const M8I2cWatch &w=m8_i2c;
 fmt_str("i2c15 scenes ");fmt_dec(w.scenes);fmt_str(" mid last ");fmt_dec(w.mid);fmt_str(" max ");fmt_dec(w.max_mid);
 fmt_str(" total ");fmt_dec(w.total_mid);fmt_str(" skip ");fmt_dec(w.skip);fmt_str(" all ");fmt_dec(input_i2c_n);fmt_line(0,0,0);
 fmt_str("i2c15 mid/scene:");
 for(uint32_t k=w.scenes>8?w.scenes-8:0;k<w.scenes;k++){fmt_str(" ");fmt_dec(w.ring[k%8]);}fmt_line(0,0,0);
 const struct mbx_ta_probe &p=w.fail_scene?mbx_tafail:mbx_probe;
 fmt_str("i2c15 fail scene ");fmt_dec(w.fail_scene);fmt_str(" mid ");fmt_dec(w.fail_mid);fmt_str(" skip ");fmt_dec(w.fail_skip);
 fmt_str(" gate 4c ");fmt_dec(p.clk4c_b>>6&1);fmt_str(">");fmt_dec(w.fail_scene?p.clk4c_f>>6&1:2);
 fmt_str(" 6c ");fmt_dec(p.clk6c_b>>12&1);fmt_str(">");fmt_dec(w.fail_scene?p.clk6c_f>>12&1:2);fmt_line(0,0,0);
}
static unsigned frame;
static bool active;
static unsigned renderMode;
// Round 10: R80 must be in RAM before the user can react (a power-off wipes
// it). Round 11: Home does not work on the user's device, so the app warm-
// resets into RetailOS on its own (reset_stockos, the launcher's StockOS
// path, which keeps 0x0AE10000): no button press anywhere in the loop.
static bool failSaved, gaveUp;   // R80 holds a failure / the GPU was given up
static uint32_t exitAt;          // USEC of the auto exit, 0 = not armed
#define EXIT_GAVE_UP_US 5000000u    // after the last re-init failed
#define EXIT_FIRST_FAIL_US 30000000u // backstop while re-inits keep going
static void arm_exit(uint32_t us){
 uint32_t at=(USEC+us)|1;
 if(!exitAt||(int32_t)(at-exitAt)<0)exitAt=at;
}
static bool exit_due(){return exitAt&&(int32_t)(USEC-exitAt)>=0;}
static uint32_t exit_secs(){return exitAt&&!exit_due()?(exitAt-USEC)/1000000u+1:0;}
// The first "frame failed" report of this app session, repeated at the end
// of every later report so Home no longer overwrites it.
static char failText[2048]; // round 14: the 1 KB copy cut the fail/probe/pool13 lines
static uint32_t failLen;
static void put_raw(const char *p,uint32_t n){while(n--&&fmt_out<fmt_end)*fmt_out++=*p++;}
// The diag lines above, as text, into report slot RPT_M8_SLOT (report.h);
// NanoOSLoader exports it as /Apps/NanoOS/reports/R80.txt on its next
// launch. Overwritten on each call: init, a failed frame, Home.
void m8_diag_report(const char *why) {
 volatile struct rpt_hdr *h=RPT_HDR;
 if(!rpt_valid()){for(unsigned i=0;i<sizeof *h/4;i++)((volatile uint32_t *)h)[i]=0;h->slot_size=RPT_SLOT_SIZE;}
 char *p0=RPT_SLOT(RPT_M8_SLOT),*keep=fmt_out;
 fmt_out=p0;fmt_end=p0+RPT_SLOT_SIZE;
 fmt_str("== VICENANO M8 GPU R");fmt_dec(RPT_M8_ID);fmt_str(" ==");fmt_line(0,0,0);
 fmt_str("event ");fmt_str(why);fmt_str(" mode ");fmt_dec(renderMode);
 fmt_str(renderMode==1?" triangle":renderMode==2?" quad":renderMode==3?" world":renderMode==4?" vc":" title");
 fmt_str(" app frame ");fmt_dec(frame);fmt_str(" t ");fmt_dec(USEC);fmt_str("us");fmt_line(0,0,0);
 fmt_str("build ");fmt_str(MBX_BUILD_TAG);fmt_str(" rearm every ");fmt_dec(MBX_REARM_EVERY);fmt_str(" scenes scenepoll ");
 fmt_str(M8_SCENE_POLL?"on":"OFF");fmt_str(" bells ");fmt_str(MBX_BELL_BATCH?"batched":"per-seg");
 fmt_str(" wake2");fmt_line(0,0,0);
 // app frame = NanoFrame calls this session (title too); fr = m8_gpu_end
 // calls since boot, sfr = this session; GPU clock word CLKCON+0x1c
 // (STOCK 2001xxxx, SAFE 1003xxxx, MAX 2000xxxx)
 fmt_str("session ");fmt_dec(m8_gpu_diag.session);fmt_str(" sfr ");fmt_dec(m8_gpu_diag.sess_frames);
 fmt_str(" inits ");fmt_dec(mbx_fail.inits);fmt_str(" rec ");fmt_dec(m8_gpu_diag.recoveries);
 // frames first after a re-arm / of those bad (black or no geometry)
 fmt_str(" rearm fr ");fmt_dec(m8_gpu_diag.rearm_frames);fmt_str(" bad ");fmt_dec(m8_gpu_diag.rearm_bad);
 fmt_str(" clk1c ");fmt_hex(*(volatile uint32_t *)0x3c50001cu,8);fmt_line(0,0,0);
 m8_diag_draw(0,0,0);
 if(!m8_gpu_diag.frames){fmt_str("(no frame rendered)");fmt_line(0,0,0);}
 fmt_str("pool pages ");fmt_dec(mbx_dbg.pages);fmt_str(" 828 ");fmt_hex(mbx_dbg.first_page,5);
 fmt_str(" 82c ");fmt_hex(mbx_dbg.last_page,5);fmt_str(" used ");fmt_dec(mbx_pool_pages_used());fmt_line(0,0,0);
 fmt_str("mbx scenes ");fmt_dec(mbx.scenes);fmt_str(" jobs ");fmt_dec(mbx.jobs);fmt_str(" to ");fmt_dec(mbx.timeouts);
 fmt_str(" status ");fmt_hex(mbx.status,8);fmt_line(0,0,0);
 {
  const struct mbx_fail_info &f=mbx_fail;
  fmt_str("scenes begun ");fmt_dec(f.scene);fmt_str(" ta max ");fmt_dec(f.ta_max_us);fmt_str("us no8 ");fmt_dec(f.no8);
  fmt_str(" aux links ");fmt_dec(mbx_aux_links());fmt_line(0,0,0);
  pool_lines();
  i2c_lines();
  if(f.fails){
   fmt_str("fail n ");fmt_dec(f.fails);fmt_str(" rsn ");fmt_dec(f.reason);fmt_str(" scene ");fmt_dec(f.fscene);
   fmt_str(" st ");fmt_hex(f.st,8);fmt_str(" pre ");fmt_hex(f.st_pre,8);fmt_line(0,0,0);
   fmt_str("fail ctx ");fmt_dec(f.ctx_res);fmt_str(" st ");fmt_hex(f.ctx_st,8);fmt_str(" ");fmt_dec(f.ctx_us);
   fmt_str("us words ");fmt_dec(f.words);fmt_str(" bells ");fmt_dec(f.bells);fmt_str(" sub ");fmt_dec(f.sub_us);fmt_str("us");fmt_line(0,0,0);
   fmt_str("fail 1020 ");fmt_hex(f.r1020,8);fmt_str(" 828 ");fmt_hex(f.r828,5);fmt_str(" 82c ");fmt_hex(f.r82c,5);
   fmt_str(" 840 ");fmt_hex(f.r840,8);fmt_str(" 85c ");fmt_hex(f.r85c,8);fmt_line(0,0,0);
   fmt_str("fail clk58 ");fmt_hex(f.clk58,8);fmt_str(" 68 ");fmt_hex(f.clk68,8);fmt_str(" 1c ");fmt_hex(f.clk1c,8);
   fmt_str(" psc14 ");fmt_hex(f.psc14,8);fmt_line(0,0,0);
   fmt_str("fail pool used ");fmt_dec(f.pool_used);fmt_str(" aux links ");fmt_dec(f.aux_links);fmt_line(0,0,0);
   fmt_str("rsn ");fmt_dec(f.reason);fmt_str(" = ");fmt_str(mbx_fail_text(f.reason));late(f);
   if(f.reason==9){fmt_str(" isp late st ");fmt_hex(f.isp_late_st,8);}fmt_line(0,0,0);
   ta_probe_lines();
  }
 }
 fmt_str("gpu frames ");fmt_dec(m8_gpu_status.frames);fmt_str(" tris ");fmt_dec(m8_gpu_status.triangles);
 fmt_str(" to ");fmt_dec(m8_gpu_status.timeouts);fmt_str(" err ");fmt_dec(m8_gpu_status.errors);
 fmt_str(" last ");fmt_hex(m8_gpu_status.last_status,8);fmt_line(0,0,0);
 bool failed=why[0]=='f'||why[0]=='g'; // frame failed / gave up / gpu off
 if(failLen){fmt_str("-- first failed frame this session --");fmt_line(0,0,0);put_raw(failText,failLen);}
 fmt_str("== END R");fmt_dec(RPT_M8_ID);fmt_str(" ==");fmt_line(0,0,0);
 uint32_t len=(uint32_t)(fmt_out-p0);
 if(failed&&!failLen){uint32_t n=len<sizeof failText?len:sizeof failText;for(uint32_t i=0;i<n;i++)failText[i]=p0[i];failLen=n;}
 fmt_out=keep;
 h->id[RPT_M8_SLOT]=RPT_M8_ID;h->len[RPT_M8_SLOT]=len;h->sum[RPT_M8_SLOT]=rpt_sum(p0,len);
 h->magic[0]=RPT_MAGIC0;h->magic[1]=RPT_MAGIC1;h->hdr_sum=rpt_hdr_sum();
 rpt_mirror_write(p0,len,h->sum[RPT_M8_SLOT]); // round 12: copies outside 0x0ae10000's block
 if(failed){failSaved=true;arm_exit(EXIT_FIRST_FAIL_US);}
}
void m8_gpu_gave_up(const char *why){
 gaveUp=true;
 m8_diag_report(why);
 arm_exit(EXIT_GAVE_UP_US);
}
// The fmt line, centered and scaled k times (the short code is read first).
static void big_line(int y,int k,uint32_t color){
 char b[48],*keep=fmt_out,*end=fmt_end;
 fmt_out=b;fmt_end=b+sizeof b;fmt_line(0,0,0);
 int n=(int)(fmt_out-b)-1;   // without the '\n'
 fmt_out=keep;fmt_end=end;
 int x0=(LCD_WIDTH-n*8*k)/2;
 for(int i=0;i<n;i++){
  const uint8_t *g=FONT_GLYPH((unsigned char)b[i]);
  for(int r=0;r<8;r++)for(int c=0;c<8;c++)if(g[r]>>c&1)
   for(int dy=0;dy<k;dy++)for(int dx=0;dx<k;dx++)lcd_pixel(x0+(i*8+c)*k+dx,y+r*k+dy,color);
 }
}
#define YELLOW 0xffffff00u
// GPU off (init failed, or every re-init used up): save R80 now, then show
// the reason code big, the save, and the auto-exit countdown.
void m8_fail_screen(void){
 if(!gaveUp)m8_gpu_gave_up("gpu off");
 const struct mbx_fail_info &f=mbx_fail;
 const struct M8GpuDiag &d=m8_gpu_diag;
 lcd_clear(0xff400000);
 lcd_center(4,"MBX unavailable",WHITE);
 if(d.frames){fmt_str("RSN ");fmt_dec(f.reason);}else fmt_str("RSN INIT");
 big_line(16,5,YELLOW);
 // the code in plain words: the photo alone must say what failed
 if(d.frames){fmt_str(mbx_fail_text(f.reason));late(f);}
 else fmt_str(d.reason?d.reason:"mbx_init failed");
 big_line(58,1,YELLOW);
 fmt_str("fail ");fmt_dec(f.fails);fmt_str(" rec ");fmt_dec(d.recoveries);fmt_str(" st ");fmt_hex(f.st,8);
 big_line(70,2,WHITE);
 fmt_str("R80 saved - REBOOT IN ");fmt_dec(exit_secs());big_line(90,2,YELLOW);
 int yl=d.frames?m8_diag_render_draw(8,118,0xffc0c0c0):(m8_diag_draw(8,118,0xffc0c0c0),196);
 if(f.fails){ // round 12 probe on the photo too: TA late/never, core id, clock gates, CPU gap
  const struct mbx_ta_probe &p=mbx_tafail;
  fmt_str("TA ");if(f.reason!=8)fmt_str("-");else if(p.late_us==MBX_NEVER)fmt_str("never");else{fmt_dec(p.late_us);fmt_str("us");}
  fmt_str(" id ");fmt_hex(p.id,8);fmt_str(" 4c ");fmt_str(p.clk4c_b==p.clk4c_f?"=":"CHG");
  fmt_str(" 6c ");fmt_str(p.clk6c_b==p.clk6c_f?"=":"CHG");fmt_str(" gap ");fmt_dec(p.gap_max_us/1000);fmt_str("ms");
  fmt_line(8,yl+2,YELLOW);
  const struct mbx_pool_watch &w=mbx_pw; // round 13: pool at the failure
  fmt_str("pool used ");fmt_dec(w.used);fmt_str(" dmax ");fmt_dec(w.dmax);fmt_str(" rearm ");fmt_dec(w.rearms);
  fmt_str(" end ");fmt_hex(w.a_end,4);fmt_line(8,yl+12,YELLOW);
 }
 lcd_center(230,"auto reboot to RetailOS - no button needed",WHITE);
 lcd_present();
}
// Bottom line of the GPU scenes: how to leave, and the save once there is one.
void m8_footer(void){
 if(!failSaved){lcd_center(224,renderMode==4?"Home: walk/turn/scene  Vol-+Vol+: back":"Volume: scene / Home: back",WHITE);return;}
 fmt_str("R80 saved - auto reboot in ");fmt_dec(exit_secs());fmt_str(" s");big_line(224,1,YELLOW);
}
// Drawn over the launcher after leaving with a failure saved (StockOS is
// preselected): the warm reset into RetailOS keeps the report RAM.
void m8_launcher_hint(void){
 if(!failSaved)return; // also reached by the pad exit (m8_app_tick 1)
 lcd_center(210,"R80 saved - Home on StockOS keeps it",YELLOW);
 lcd_present();
}
uint32_t m8_time_us(void) { return USEC; }
void m8_title(unsigned tick) {
 (void)tick; // no animation: diagnostics must stay readable and are redrawn in full every call
 lcd_clear(0xff000000);
 m8_diag_draw(8,8,0xffffff00);
 lcd_center(184, "ViceNano M8 / NanoOS", WHITE);
 lcd_center(200, "Title - press Volume to start", WHITE);
 lcd_center(224, "Home or Vol- + Vol+ : back", WHITE);
 lcd_present();
}
void m8_app_enter(void) {
 frame=0;renderMode=0;failLen=0;failSaved=gaveUp=false;exitAt=0;m8PadExit=false;
 ++m8_gpu_diag.session;m8_gpu_diag.sess_frames=0;m8_gpu_diag.recoveries=0;
 active=NanoInit(LCD_WIDTH,LCD_HEIGHT);
 if(active) { if(!NanoFrame(frame++)) m8_title(frame); }
 m8_diag_report("init");
}
// The auto exit: R80 ("auto reboot", first failed frame appended) goes to RAM,
// then the watchdog reset into RetailOS, the same reset_stockos() the
// launcher runs for StockOS. D-cache is off in NanoOS, so the stores are
// already in DRAM.
// Round 12: round 11 reset with the MBX still mid-job (a hung TA/ISP bus
// master, its MMU on) and the report was gone after the reboot; the manual
// path that kept it ran NanoExit -> m8_gpu_shutdown() (register restore +
// 0x1020 soft reset, plain MMIO writes, no waits) and sat idle in the
// launcher before the reset. Do the same quiesce here (not the whole
// NanoExit: only the GPU part touches hardware), let the LCD DMA finish,
// rewrite the report (+ mirrors/canaries, report.h) and settle before the
// reset.
__attribute__((noreturn)) static void reboot_retail(void){
 m8_diag_report("auto reboot");
 lcd_clear(0xff000000);
 fmt_str("REBOOTING");big_line(80,4,YELLOW);
 lcd_center(130,"R80 saved - starting RetailOS",WHITE);
 lcd_center(146,"GPU stopped, waiting 0.5 s",WHITE);
 lcd_present();
 m8_gpu_shutdown();active=false;
 for(uint32_t t0=USEC;lcd_busy()&&USEC-t0<1000000u;)lcd_service();
 m8_diag_report("auto reboot");
 for(uint32_t t0=USEC;USEC-t0<500000u;)lcd_service();
 reset_stockos();
}
int m8_app_tick(unsigned keys) {
 // Button edges plus held levels (Vol-/Vol+ = keys 2/4); NanoFrame reads no
 // CPad (touch contacts can feed m8_walk's camera state once delivered).
 if(!m8_walk(input_held(),keys)){
  if(keys&2)renderMode=(renderMode+M8_MODES-1)%M8_MODES;
  if(keys&4)renderMode=(renderMode+1)%M8_MODES;
 }
 m8_render_mode(renderMode);
 if(active && lcd_ready()) { if(!NanoFrame(frame++)) m8_title(frame); }
 if(exit_due())reboot_retail();
 // the pad's exit: the launcher's tick path (m8_app_exit, menu; it
 // preselects StockOS)
 if(m8PadExit){m8PadExit=false;return 1;}
 return 0;
}
// The report goes first: a hang in the GPU shutdown cannot lose it.
// 8 = BUTTON_CHORD (nanoos/src/input.h): the Vol-+Vol+ chord always leaves
int m8_app_home(unsigned keys){return !(keys&8u)&&m8_walk_home();}
int m8_app_exit(void) {
 m8_diag_report("home");NanoExit();active=false;exitAt=0;
 return failSaved;
}
