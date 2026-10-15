#pragma once
#include "../../rw.h"
extern "C" {
#include "gl.h"
}
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0bc0
#endif
namespace rw { namespace gles1 {
extern float view[16], projection[16];
extern int width,height;
GLenum primitive(PrimitiveType);
} }
