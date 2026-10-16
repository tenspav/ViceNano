#include "nanoos_shim.h"
M8GpuStatus m8_gpu_status;
int m8_gpu_init(void) { ++m8_gpu_status.errors; return -1; }
int m8_gpu_begin(uint32_t) { return -1; }
int m8_gpu_triangle(const M8GpuVertex *) { return -1; }
int m8_gpu_end(void) { return -1; }
void m8_gpu_shutdown(void) {}

int m8_gpu_texture_create(unsigned,unsigned,unsigned,const uint16_t *){return -1;}
void m8_gpu_texture_destroy(int){}
int m8_gpu_textured_triangle(int,const M8GpuTexVertex *){return -1;}
