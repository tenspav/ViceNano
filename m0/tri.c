/* RetailOS owns context creation and presentation, as in probe/gltest.
 * No GL calls before gl_app_frame; no device or context memory pokes. */
#include "gl.h"
#include "hb_gl_surface.h"

void gl_app_init(void) {}

void gl_app_frame(int w, int h, uint32_t frame)
{
    static const GLfloat vertices[] = {
        -0.65f, -0.55f, 0.f, 0.65f, -0.55f, 0.f, 0.f, 0.75f, 0.f
    };
    static const GLubyte colors[] = {
        255, 48, 48, 255, 48, 255, 48, 255, 48, 48, 255, 255
    };
    GLfloat aspect;
    if (w <= 0 || h <= 0) return;
    aspect = (GLfloat)w / (GLfloat)h;
    glViewport(0, 0, w, h);
    glClearColor(0.05f, 0.08f, 0.25f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-0.5f * aspect, 0.5f * aspect, -0.5f, 0.5f, 1.f, 10.f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.f, 0.f, -3.5f);
    glRotatef((GLfloat)((frame % 180u) * 2u), 0.f, 0.f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}
