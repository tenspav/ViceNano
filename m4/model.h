#pragma once
#include <stdint.h>
struct NanVertex { float x,y,z,u,v; uint8_t r,g,b,a; };
struct NanTexture { int width,height,format; const uint16_t *pixels; const char *name; };
struct NanPart { int vertices,triangles,texture; const NanVertex *data; const uint16_t *indices; };
