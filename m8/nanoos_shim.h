#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void m8_app_enter(void);
void m8_app_tick(unsigned keys);
void m8_app_exit(void);
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
void m8_gpu_shutdown(void);
/* 0 title, 1 triangle test, 2 textured quad, 3 embedded world preview. */
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
