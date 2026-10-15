extern "C" {
#include "gl.h"
}
#include <string.h>
unsigned mock_draws,mock_uploads,mock_deletes,mock_gl_calls,mock_buffers_created,mock_buffers_deleted,mock_upload_bytes;
float mock_projection[16];static GLenum matrix_mode;float mock_clip[12];static int vertex_size,vertex_stride;static const void *vertex_ptr;
void glClear(GLbitfield mask){mock_gl_calls++;}
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a){mock_gl_calls++;}
void glClearDepthf(GLclampf depth){mock_gl_calls++;}
void glClearStencil(GLint s){mock_gl_calls++;}
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a){mock_gl_calls++;}
void glEnable(GLenum cap){mock_gl_calls++;}
void glDisable(GLenum cap){mock_gl_calls++;}
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h){mock_gl_calls++;}
void glMatrixMode(GLenum mode){mock_gl_calls++;matrix_mode=mode;}
void glLoadIdentity(void){mock_gl_calls++;}
void glLoadMatrixf(const GLfloat *m){mock_gl_calls++;if(matrix_mode==GL_PROJECTION)memcpy(mock_projection,m,64);}
void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f){mock_gl_calls++;}
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z){mock_gl_calls++;}
void glTranslatef(GLfloat x, GLfloat y, GLfloat z){mock_gl_calls++;}
void glScalef(GLfloat x, GLfloat y, GLfloat z){mock_gl_calls++;}
void glPixelStorei(GLenum pname, GLint param){mock_gl_calls++;}
void glTexParameteri(GLenum target, GLenum pname, GLint param){mock_gl_calls++;}
void glEnableClientState(GLenum array){mock_gl_calls++;}
void glDisableClientState(GLenum array){mock_gl_calls++;}
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr){mock_gl_calls++;vertex_size=size;vertex_stride=stride;vertex_ptr=ptr;}
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr){mock_gl_calls++;}
void glDrawArrays(GLenum mode, GLint first, GLsizei count){mock_gl_calls++;mock_draws++;if(vertex_size==4)for(int i=0;i<3;i++)memcpy(mock_clip+4*i,(const char*)vertex_ptr+i*vertex_stride,16);}
void glGenBuffers(GLsizei n, GLuint *buffers){mock_gl_calls++;for(int i=0;i<n;i++)buffers[i]=++mock_buffers_created;}
void glBindBuffer(GLenum target, GLuint buffer){mock_gl_calls++;}
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage){mock_gl_calls++;}
void glBufferSubData(GLenum target, GLsizeiptr offset, GLsizeiptr size, const GLvoid *data){mock_gl_calls++;}
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a){mock_gl_calls++;}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a){mock_gl_calls++;}
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr){mock_gl_calls++;}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr){mock_gl_calls++;}
void glBlendFunc(GLenum sfactor, GLenum dfactor){mock_gl_calls++;}
void glDepthFunc(GLenum func){mock_gl_calls++;}
void glShadeModel(GLenum mode){mock_gl_calls++;}
void glCullFace(GLenum mode){mock_gl_calls++;}
void glLineWidth(GLfloat width){mock_gl_calls++;}
void glPushMatrix(void){mock_gl_calls++;}
void glPopMatrix(void){mock_gl_calls++;}
void glDepthMask(GLboolean flag){mock_gl_calls++;}
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height){mock_gl_calls++;}
void glActiveTexture(GLenum texture){mock_gl_calls++;}
void glClientActiveTexture(GLenum texture){mock_gl_calls++;}
void glMultMatrixf(const GLfloat *m){mock_gl_calls++;}
void glLoadMatrixx(const GLfixed *m){mock_gl_calls++;}
void glDeleteBuffers(GLsizei n, const GLuint *buffers){mock_gl_calls++;mock_buffers_deleted+=n;}
void glMaterialf(GLenum face, GLenum pname, GLfloat param){mock_gl_calls++;}
void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f){mock_gl_calls++;}
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz){mock_gl_calls++;}
void glAlphaFunc(GLenum func, GLclampf ref){mock_gl_calls++;}
void glGenTextures(GLsizei n, GLuint *textures){mock_gl_calls++;for(int i=0;i<n;i++)textures[i]=i+1;}
void glBindTexture(GLenum target, GLuint texture){mock_gl_calls++;}
void glDeleteTextures(GLsizei n, const GLuint *textures){mock_gl_calls++;mock_deletes++;}
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                  GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels){mock_gl_calls++;mock_uploads++;mock_upload_bytes+=width*height*2;}
void glTexEnvi(GLenum target, GLenum pname, GLint param){mock_gl_calls++;}
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params){mock_gl_calls++;}
void glTexEnviv(GLenum target, GLenum pname, const GLint *params){mock_gl_calls++;}
void glLightf(GLenum light, GLenum pname, GLfloat param){mock_gl_calls++;}
void glLightfv(GLenum light, GLenum pname, const GLfloat *params){mock_gl_calls++;}
void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params){mock_gl_calls++;}
void glGetIntegerv(GLenum pname, GLint *params){mock_gl_calls++;}
void glGetFloatv(GLenum pname, GLfloat *params){mock_gl_calls++;}
GLenum glGetError(void){mock_gl_calls++;return 0;}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices){mock_gl_calls++;mock_draws++;if(vertex_size==4)for(int i=0;i<3;i++)memcpy(mock_clip+4*i,(const char*)vertex_ptr+i*vertex_stride,16);}
void hb_gl_drawelements_native(int on){mock_gl_calls++;}
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                            GLsizei width, GLsizei height, GLint border,
                            GLsizei imageSize, const GLvoid *data){mock_gl_calls++;}
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const GLvoid *pixels){mock_gl_calls++;}
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, GLvoid *pixels){mock_gl_calls++;}
