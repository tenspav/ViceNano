#include "nano.h"
#include "stream.h"
#include "input.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"

extern "C" {
#include "hb_gl_surface.h"
#include "hb_heap.h"
#include "gl.h"
#include "hb_sdk.h"
#include "hb_touch_mb.h"
#include "hh_bt.h"
void nano_run_constructors();
}

using namespace rw;

static unsigned errors, loadedHeap, heapLow = ~0u;
static bool ready, balanced = true;

// Preallocated overlay buffer for non-blocking single-draw 2D orthographic rendering
struct OverlayVertex {
 float x, y;
 uint8_t r, g, b, a;
};
static OverlayVertex overlay[2048 * 6];
static unsigned quads;

static void flushOverlay2D();

static void tile(int x, int y, int w, int h, unsigned r, unsigned g, unsigned b) {
 if (quads >= 2048) flushOverlay2D();
 OverlayVertex *v = overlay + quads * 6;
 float fx = (float)x, fy = (float)y, fw = (float)w, fh = (float)h;
 uint8_t ur = (uint8_t)r, ug = (uint8_t)g, ub = (uint8_t)b;
 v[0] = {fx,      fy,      ur, ug, ub, 255};
 v[1] = {fx + fw, fy,      ur, ug, ub, 255};
 v[2] = {fx + fw, fy + fh, ur, ug, ub, 255};
 v[3] = {fx,      fy,      ur, ug, ub, 255};
 v[4] = {fx + fw, fy + fh, ur, ug, ub, 255};
 v[5] = {fx,      fy + fh, ur, ug, ub, 255};
 quads++;
}

static void label(int x, int y, const char *text) {
 for (; *text; text++, x += 6) {
  const uint8_t *glyph = hb_font8x8_glyph(*text);
  if (!glyph) continue;
  for (int row = 0; row < 8; row++) {
   uint8_t bits = glyph[row];
   if (!bits) continue;
   int col = 0;
   while (col < 6) {
    if (bits & (0x80 >> col)) {
     int start = col;
     while (col < 6 && (bits & (0x80 >> col))) col++;
     tile(y + row, x + start, 1, col - start, 220, 230, 240);
    } else {
     col++;
    }
   }
  }
 }
}

static void flushOverlay2D() {
 if (quads == 0) return;
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 glOrthof(0, 240.0f, 432.0f, 0, -1.0f, 1.0f);
 glMatrixMode(GL_MODELVIEW);
 glLoadIdentity();
 glDisable(GL_DEPTH_TEST);
 glDepthMask(GL_FALSE);
 glDisable(GL_CULL_FACE);
 glDisable(GL_BLEND);
 glDisable(GL_TEXTURE_2D);
 glBindBuffer(GL_ARRAY_BUFFER, 0);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
 glEnableClientState(GL_VERTEX_ARRAY);
 glVertexPointer(2, GL_FLOAT, sizeof(OverlayVertex), &overlay[0].x);
 glEnableClientState(GL_COLOR_ARRAY);
 glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(OverlayVertex), &overlay[0].r);
 glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 glDisableClientState(GL_NORMAL_ARRAY);
 glDrawArrays(GL_TRIANGLES, 0, quads * 6);
 quads = 0;
}

// Profiling metrics
static uint32_t lastFrameUs = 0;
static uint32_t avgFrameUs = 33333; // initial estimate ~30 FPS
static uint32_t lastInputUs, lastAudioUs, lastStreamUs, lastDrawUs, lastOverlayUs;

extern "C" void gl_app_init() {
 nano_run_constructors();
 hb_touch_init();
 hh_bt_init();
}

extern "C" void gl_app_frame(int w, int h, uint32_t frame) {
 if (w <= 0 || h <= 0) return;
 uint32_t frameStart = hb_time_uptime_us();

 if (!ready) {
  if (!NanoInit(w, h)) { errors++; return; }
  ready = true;
 }

 // 1. Input Processing
 uint32_t tInput0 = hb_time_uptime_us();
 hb_touch_t raw[HB_MAX_FINGERS];
 Contact contacts[8];
 int count = 0;
 hb_touch_drain_all();
 int n = hb_touch_poll_multi(raw);
 static Contact held[8];
 static bool down[8];
 static uint32_t seenMs[8];
 uint32_t nowMs = hb_time_uptime_ms();
 bool seen[8] = {};
 for (int i = 0; i < n; i++) if (raw[i].touch_id < 8) {
  unsigned id = raw[i].touch_id;
  seen[id] = true;
  seenMs[id] = nowMs;
  down[id] = raw[i].status != 1;
  held[id] = {int(id), raw[i].y, raw[i].x};
 }
 for (int i = 0; i < 8; i++) if (!seen[i] && nowMs - seenMs[i] > 1000) down[i] = false;
 hb_touch_state_t mailbox;
 if (hb_touch_mb_read(&mailbox)) {
  down[0] = mailbox.down;
  held[0] = {0, mailbox.y, mailbox.x};
 }
 for (int i = 0; i < 8; i++) if (down[i]) contacts[count++] = held[i];
 TouchUpdate(contacts, count);

 static bool powerAsked;
 if (pad.NewState.Start && !powerAsked) { powerAsked = true; hh_bt_power_on(); }
 hh_bt_tick();
 hid_pad hp;
 bool available = hh_bt_pad(&hp);
 uint8_t peer[6];
 const uint8_t nanoPeer[6] = {2, 0x4e, 0x50, 0x41, 0x44, 1};
 if (available && hh_bt_peer(peer) && !memcmp(peer, nanoPeer, 6)) {
  hp.axis[3] = hp.axis[2];
  hp.axis[4] = hp.axis[5];
  hp.axis[2] = hp.axis[5] = 0;
 }
 pad.Update(hh_bt_state() == HH_CONNECTED, available ? &hp : 0);
 lastInputUs = hb_time_uptime_us() - tInput0;

 // 2. Audio Processing & DMA Mixing
 uint32_t tAudio0 = hb_time_uptime_us();
 AudioTestTick();
 lastAudioUs = hb_time_uptime_us() - tAudio0;

 // 3. 3D Scene Rendering & City Streaming
 uint32_t tRender0 = hb_time_uptime_us();
 if (!NanoFrame(frame)) { errors++; return; }
 while (glGetError() != GL_NO_ERROR) errors++;
 lastStreamUs = nanoStreamUs;
 lastDrawUs = nanoDrawUs;

 loadedHeap = hb_os_heap_free();
 if (loadedHeap < heapLow) heapLow = loadedHeap;
 balanced = balanced && !stream.failed && !NanoFailures() && NanoPeak() < HeapBudget;

 // 4. 2D Overlay, FPS Counter & Profiling
 uint32_t tOverlay0 = hb_time_uptime_us();
 quads = 0;

 // Compute FPS
 uint32_t dtFrame = (lastFrameUs > 0) ? (frameStart - lastFrameUs) : 33333;
 lastFrameUs = frameStart;
 if (dtFrame > 1000 && dtFrame < 1000000) {
  avgFrameUs = (avgFrameUs * 7 + dtFrame) / 8; // smooth 8-frame moving average
 }
 uint32_t fpsX10 = (avgFrameUs > 0) ? (10000000u / avgFrameUs) : 0;
 uint32_t fpsInt = fpsX10 / 10;
 uint32_t fpsFrac = fpsX10 % 10;

 // Draw On-Screen FPS Counter (top-left in landscape, with background plate)
 char fpsStr[16];
 snprintf(fpsStr, sizeof(fpsStr), "FPS %u.%u", (unsigned)fpsInt, (unsigned)fpsFrac);
 tile(8, 8, 12, 54, 15, 20, 35);
 label(10, 10, fpsStr);

 if (hh_bt_state() != HH_CONNECTED) {
  const char *names[] = {"PAUSE", "BACK", "PRE", "NXT", "AIM", "ENTER", "FIRE", "JUMP", "RUN", "DUCK"};
  for (int b = 0; b < 10; b++) {
   const Button &v = buttons[b];
   bool pressed = reinterpret_cast<int16_t*>(&pad.NewState)[v.field] != 0;
   tile(v.y, v.x, v.h, v.w, pressed ? 240 : 50, pressed ? 180 : 75, 110);
   label(v.x + 3, v.y + v.h / 2, names[b]);
  }
  tile(160, 40, 4, 60, 90, 180, 160);
  tile(135, 20, 50, 4, 90, 180, 160);
 }

 // Status indicators
 tile(10, 130, 25, 12, errors ? 255 : 0, errors ? 0 : 255, 0);
 tile(45, 130, 25, 12, balanced ? 0 : 255, 255, 0);
 tile(80, 130, 25, 12, gles1::unsupportedStates ? 255 : 0, 255, 0);

 // Machine-readable bit rows (rows 0-15 preserved from M7, rows 16-20 for M8 performance)
 unsigned values[] = {
  inputEvents,
  hh_bt_reports(),
  unsigned(int(playerX * 16)),
  unsigned(int(playerZ * 16)),
  unsigned(int(cameraYaw * 100)),
  unsigned(int(cameraPitch * 100)),
  unsigned(NanoLive()),
  loadedHeap,
  unsigned(NanoPeak()),
  errors,
  NanoFailures(),
  unsigned(hh_bt_state()),
  pad.NewState.Cross ? 1u : 0u,
  unsigned(pad.NewState.LeftStickX),
  unsigned(pad.NewState.LeftStickY),
  frame,
  fpsX10,
  lastDrawUs,
  lastStreamUs,
  lastAudioUs,
  lastInputUs
 };

 // Single solid background plate covering all 21 bit rows
 tile(8, 148, 32 * 7, 21 * 4, 0, 0, 0);

 for (unsigned row = 0; row < 21; row++) {
  for (unsigned i = 0; i < 32; i++) {
   unsigned b = (values[row] >> i) & 1;
   if (b) {
    tile(8 + i * 7, 148 + row * 4, 5, 3, 255, 255, 255);
   }
  }
 }

 flushOverlay2D();
 lastOverlayUs = hb_time_uptime_us() - tOverlay0;

 uint32_t totalFrameUs = hb_time_uptime_us() - frameStart;

 // Periodic profiling log every 30 frames
 if (frame % 30 == 0) {
  fprintf(stderr, "[M8-PROFILE] frame=%u fps=%u.%u total=%u us (draw=%u stream=%u audio=%u input=%u overlay=%u)\n",
          (unsigned)frame, (unsigned)fpsInt, (unsigned)fpsFrac, (unsigned)totalFrameUs,
          (unsigned)lastDrawUs, (unsigned)lastStreamUs, (unsigned)lastAudioUs,
          (unsigned)lastInputUs, (unsigned)lastOverlayUs);
 }
}
