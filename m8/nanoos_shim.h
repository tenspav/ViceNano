#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void m8_app_enter(void);
/* 1 = leave now (auto exit after a saved failure) */
int m8_app_tick(unsigned keys);
/* 1 = Home kept by the app (walk/turn/scene in mode 4); the chord leaves */
int m8_app_home(unsigned keys);
/* 1 = R80 holds a failure of this session (preselect StockOS) */
int m8_app_exit(void);
void m8_launcher_hint(void);
uint32_t m8_time_us(void);
void m8_title(unsigned tick);
#ifdef __cplusplus
}
#endif

/* Replaceable NanoOS GPU driver contract, ABI version 1. Screen coordinates
 * are landscape 432x240; colors are 0xAARRGGBB. No pointer is retained.
 * All calls return 0 on success or a negative error. end never presents. */
#ifdef __cplusplus
extern "C" {
#endif
#define M8_GPU_ABI_VERSION 2
struct M8GpuVertex { float x, y, z; uint32_t argb; };
struct M8GpuStatus { uint32_t frames, triangles, timeouts, errors, last_status; };
extern struct M8GpuStatus m8_gpu_status;
int m8_gpu_init(void);
int m8_gpu_begin(uint32_t clear_argb);
int m8_gpu_triangle(const struct M8GpuVertex vertices[3]);
int m8_gpu_end(void);
/* render into the LCD back buffer (no sentinel, no copy, no pixel classes) */
extern int m8_gpu_direct;
void m8_gpu_shutdown(void);
/* 0 title, 1 triangle test, 2 textured quad, 3 embedded world preview,
 * 4 Vice City street corner (asset pack, nanoos_world.cpp). */
#define M8_MODES 5
void m8_render_mode(unsigned mode);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* API v2 additions. Upload copies linear 16-bit pixels; 1 RGB565, 2 ARGB4444.
 * Positive texture handles; negative error. Ownership ends at destroy/shutdown.
 * UVs use normalized coordinates with nearest/repeat sampling. */
struct M8GpuTexVertex { struct M8GpuVertex p; float u,v; };
int m8_gpu_texture_create(unsigned width,unsigned height,unsigned format,const uint16_t *pixels);
void m8_gpu_texture_destroy(int texture);
int m8_gpu_textured_triangle(int texture,const struct M8GpuTexVertex vertices[3]);
#ifdef __cplusplus
}
#endif

/* On-screen bring-up diagnostics, filled by the GPU backend (any backend). */
#ifdef __cplusplus
extern "C" {
#endif
struct M8GpuDiag {
 uint32_t called, init_ret, id, rev, sig, target; const char *reason;
 /* last m8_gpu_end(): stage reached, TA/ISP done bits seen, their times and
  * +0x12c words, render registers, TA-written PB words, and the target's
  * pixels by writer (gpu = drawn, bg = ISP background, untouched = CPU
  * sentinel left as is; black = gpu pixels that are 0x000000) */
 const char *stage;
 uint32_t frames, segments, ta_ok, isp_ok, ta_us, isp_us, render_us,
          ta_status, isp_status, timeouts, fb_va, fmt, stride, pb_used,
          px_gpu, px_bg, px_untouched, px_black, center;
 /* recoveries: m8_gpu_recover() re-inits this app session; session: app
  * enters since boot; sess_frames: m8_gpu_end calls this session (frames is
  * since boot) */
 uint32_t recoveries, session, sess_frames;
 /* black-frame check (modes 1-3, pixel classes): frames rendered first
  * after a pool re-arm (core_init + prefix), and those with under half the
  * pixels from geometry or over half black. 0 bad on the device = the
  * NanoEmu-only parse bug (nano7_mbxr.c state-only header) */
 uint32_t rearm_frames, rearm_bad;
};
extern struct M8GpuDiag m8_gpu_diag;
/* Round 15 hypothesis-1 FALLBACK test (GPU upset by the PMU I2C traffic,
 * and its CLKCON +0x4c/+0x6c gate cycles, that the button sampling does
 * every 16 triangles inside a scene). M8_SCENE_POLL 1 (default): the
 * round-14 every-16 sampling, so the primary round-15 fix (one doorbell
 * per segment, mbx.h MBX_BELL_BATCH) is tested alone. 0 = no input
 * sampling between m8_gpu_begin and the end of the scene's job.
 * Per scene (m8_gpu_begin .. job done): mid = PMU I2C reads issued inside
 * it (input_i2c_n), skip = samplings skipped; ring of the last 8 completed
 * scenes; fail_* = the first failed scene of the session. */
#ifndef M8_SCENE_POLL
#define M8_SCENE_POLL 1
#endif
struct M8I2cWatch {
 uint32_t i2c_b, mid, skip, max_mid, total_mid, scenes, ring[8],
          fail_scene, fail_mid, fail_skip;
};
extern struct M8I2cWatch m8_i2c;
void m8_diag_draw(int x, int y, uint32_t color);
/* Render-stage lines of the last frame (none before the first frame);
 * returns the y below the last line drawn. */
int m8_diag_render_draw(int x, int y, uint32_t color);
/* All diag lines as text into the NanoOS RAM report slot (R80.txt). */
void m8_diag_report(const char *why);
/* After a failed frame: re-run the MBX init (reset, MMU, allocator, context)
 * so the next frame can render. 0 ok; -1 when it failed or the session
 * already used its re-inits (then the GPU stays off until the next enter). */
int m8_gpu_recover(void);
/* GPU off for this session: write R80 now and arm the 5 s auto exit */
void m8_gpu_gave_up(const char *why);
/* "MBX unavailable" screen: reason code, "report saved", countdown */
void m8_fail_screen(void);
/* bottom line of the GPU scenes */
void m8_footer(void);
#ifdef __cplusplus
}
#endif
