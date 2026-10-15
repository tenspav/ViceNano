#include "gl.h"
#define GL_ALPHA_TEST 0x0bc0
#include "hb_gl_surface.h"
#include "hb_heap.h"

/* Separate vertices at each face give independent UVs and outward CCW winding. */
static const GLfloat vertices[] = {
 -1,-1,1, 1,-1,1, 1,1,1, -1,1,1,
 1,-1,-1, -1,-1,-1, -1,1,-1, 1,1,-1,
 1,-1,1, 1,-1,-1, 1,1,-1, 1,1,1,
 -1,-1,-1, -1,-1,1, -1,1,1, -1,1,-1,
 -1,1,1, 1,1,1, 1,1,-1, -1,1,-1,
 -1,-1,-1, 1,-1,-1, 1,-1,1, -1,-1,1
};
static const GLfloat uv[] = {
 0,0,1,0,1,1,0,1, 0,0,1,0,1,1,0,1, 0,0,1,0,1,1,0,1,
 0,0,1,0,1,1,0,1, 0,0,1,0,1,1,0,1, 0,0,1,0,1,1,0,1
};
static const unsigned short indices[] = {
 0,1,2,0,2,3, 4,5,6,4,6,7, 8,9,10,8,10,11,
 12,13,14,12,14,15, 16,17,18,16,18,19, 20,21,22,20,22,23
};
static GLuint texture, buffers[2];
static unsigned errors, draws, base_heap, last_heap;
static int ready;
static void quad(float x, float y, float w, float h, float r, float g, float b, float a)
{
 GLfloat v[]={x,y,0,x+w,y,0,x+w,y+h,0,x,y+h,0};
 static const unsigned short ix[]={0,1,2,0,2,3};
 glColor4f(r,g,b,a); glVertexPointer(3,GL_FLOAT,0,v);
 glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_SHORT,ix); draws++;
}
void gl_app_init(void) {}
void gl_app_frame(int w,int h,uint32_t frame)
{
 if(w<=0||h<=0)return;
 if(!ready){
  unsigned short pixels[64]; unsigned i;
  for(i=0;i<64;i++) pixels[i]=(((i%8)/2+(i/8)/2)&1)?0xffff:0xf800;
  hb_gl_drawelements_native(1);
  glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,8,8,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,pixels);
  glGenBuffers(2,buffers);glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);glBufferData(GL_ARRAY_BUFFER,sizeof(uv),uv,GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER,0); ready=1;
 }
 glViewport(0,0,w,h);glClearColor(.05f,.08f,.25f,1);
 glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();
 glFrustumf(-.5f*(float)w/h,.5f*(float)w/h,-.5f,.5f,1,20);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(0,0,-5.5f);
 glRotatef((float)(frame%360),0,1,0);glRotatef(25,1,0,0);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
 glDisable(GL_LIGHTING);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glColor4f(1,1,1,1);
 glDisableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glVertexPointer(3,GL_FLOAT,0,0);
 glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);glTexCoordPointer(2,GL_FLOAT,0,0);
 glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_SHORT,indices);draws++;
 glBindBuffer(GL_ARRAY_BUFFER,0);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 glDisable(GL_TEXTURE_2D);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,240,432,0,-1,1);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 /* Alpha rejection: magenta must disappear, cyan must survive. */
 glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.5f);
 quad(10,360,40,30,1,0,1,.25f);quad(60,360,40,30,0,1,1,1);glDisable(GL_ALPHA_TEST);
 /* Blue under red at half alpha: center must be purple. */
 quad(110,360,40,30,0,0,1,1);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 quad(110,360,40,30,1,0,0,.5f);glDisable(GL_BLEND);
 while(glGetError()!=GL_NO_ERROR)errors++;
 if(frame>300 && !base_heap)base_heap=hb_os_heap_free();
 last_heap=hb_os_heap_free();
 quad(10,400,30,12,errors?1:0,errors?0:1,0,1);
 quad(50,400,30,12,(base_heap && last_heap+4096<base_heap)?1:0,1,0,1);
 /* 8-bit draw counter in black/white cells: proves repeated native calls. */
 for(unsigned i=0;i<8;i++){float bit=(draws>>i)&1;quad(90+i*16,400,12,12,bit,bit,bit,1);}
 for(unsigned i=0;i<32;i++){
  float bit=(base_heap>>i)&1;quad(8+i*7,420,5,4,bit,bit,bit,1);
  bit=(last_heap>>i)&1;quad(8+i*7,426,5,4,bit,bit,bit,1);
 }

}
