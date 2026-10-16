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

// 2D overlay in three draws: flat tiles, glyph quads (font atlas) and the
// telemetry bit rows (one quad over a bit texture). Per-pixel-run tiles used to
// submit more vertices than the 3D world.
struct OverlayVertex {
 float x, y;
 uint8_t r, g, b, a;
 float u, v;
};
static const unsigned MaxTiles = 256, MaxGlyphs = 256;
static OverlayVertex overlay[MaxTiles * 6], glyphs[MaxGlyphs * 6];
static unsigned quads, glyphQuads;
static GLuint fontTexture, telemetryTexture;

// Telemetry rows render as one NEAREST-sampled quad: texel (bit, row) covers the
// 7x4 cell whose pixel (10+bit*7, 149+row*4) verify.py samples.
static const int TelemetryX = 8, TelemetryY = 148, TelemetryTexW = 32, TelemetryTexH = 64;
static uint16_t telemetryTexels[TelemetryTexW * TelemetryTexH];
static unsigned telemetryRows;

static void quad(OverlayVertex *v, float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b,
                 float u0, float v0, float u1, float v1) {
 v[0] = {x,     y,     r, g, b, 255, u0, v0};
 v[1] = {x + w, y,     r, g, b, 255, u1, v0};
 v[2] = {x + w, y + h, r, g, b, 255, u1, v1};
 v[3] = {x,     y,     r, g, b, 255, u0, v0};
 v[4] = {x + w, y + h, r, g, b, 255, u1, v1};
 v[5] = {x,     y + h, r, g, b, 255, u0, v1};
}

static void tile(int x, int y, int w, int h, unsigned r, unsigned g, unsigned b) {
 if (quads >= MaxTiles) return;
 quad(overlay + quads++ * 6, (float)x, (float)y, (float)w, (float)h, (uint8_t)r, (uint8_t)g, (uint8_t)b, 0, 0, 0, 0);
}

// Font atlas: ASCII 32..127 as 8x8 cells in a 128x64 RGBA4444 texture.
static void createFontTexture() {
 static uint16_t texels[128 * 64];
 for (int c = 32; c < 128; c++) {
  const uint8_t *glyph = hb_font8x8_glyph((char)c);
  if (!glyph) continue;
  int cx = (c - 32) % 16 * 8, cy = (c - 32) / 16 * 8;
  for (int row = 0; row < 8; row++)
   for (int col = 0; col < 8; col++)
    if (glyph[row] & (0x80 >> col)) texels[(cy + row) * 128 + cx + col] = 0xffff;
 }
 glGenTextures(1, &fontTexture);
 glBindTexture(GL_TEXTURE_2D, fontTexture);
 glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 64, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, texels);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// Text runs along +y (landscape); glyph rows map to +x, glyph columns to +y.
static void label(int x, int y, const char *text) {
 for (; *text; text++, x += 6) {
  int c = (unsigned char)*text;
  if (c < 33 || c > 127 || glyphQuads >= MaxGlyphs) continue;
  float cu = (c - 32) % 16 * 8 / 128.f, cv = (c - 32) / 16 * 8 / 64.f;
  float u1 = cu + 6 / 128.f, v1 = cv + 8 / 64.f;
  OverlayVertex *v = glyphs + glyphQuads++ * 6;
  float gx = (float)y, gy = (float)x;
  v[0] = {gx,     gy,     220, 230, 240, 255, cu, cv};
  v[1] = {gx + 8, gy,     220, 230, 240, 255, cu, v1};
  v[2] = {gx + 8, gy + 6, 220, 230, 240, 255, u1, v1};
  v[3] = {gx,     gy,     220, 230, 240, 255, cu, cv};
  v[4] = {gx + 8, gy + 6, 220, 230, 240, 255, u1, v1};
  v[5] = {gx,     gy + 6, 220, 230, 240, 255, u1, cv};
 }
}

static void createTelemetryTexture() {
 glGenTextures(1, &telemetryTexture);
 glBindTexture(GL_TEXTURE_2D, telemetryTexture);
 glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TelemetryTexW, TelemetryTexH, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, telemetryTexels);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void setTelemetry(const unsigned *values, unsigned rows) {
 if (rows > TelemetryTexH) rows = TelemetryTexH;
 for (unsigned row = 0; row < rows; row++)
  for (unsigned i = 0; i < 32; i++)
   telemetryTexels[row * TelemetryTexW + i] = (values[row] >> i) & 1 ? 0xffff : 0;
 telemetryRows = rows;
}

// Re-specify in place at frame start: uploads between draws can reset the
// RetailOS GL driver (mk64-renderer-tricks.md), so rows lag one frame.
static void uploadTelemetry() {
 glBindTexture(GL_TEXTURE_2D, telemetryTexture);
 glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TelemetryTexW, TelemetryTexH, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, telemetryTexels);
}

static const float s_orthoProj[16] = {
 2.0f / 240.0f, 0.0f, 0.0f, 0.0f,
 0.0f, -2.0f / 432.0f, 0.0f, 0.0f,
 0.0f, 0.0f, -1.0f, 0.0f,
 -1.0f, 1.0f, 0.0f, 1.0f
};

static void drawOverlay(const OverlayVertex *v, unsigned count) {
 glVertexPointer(2, GL_FLOAT, sizeof(OverlayVertex), &v->x);
 glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(OverlayVertex), &v->r);
 glTexCoordPointer(2, GL_FLOAT, sizeof(OverlayVertex), &v->u);
 glDrawArrays(GL_TRIANGLES, 0, count);
}

static void flushOverlay2D() {
 glMatrixMode(GL_PROJECTION);
 glLoadMatrixf(s_orthoProj);
 glMatrixMode(GL_MODELVIEW);
 glLoadIdentity();
 glDisable(GL_DEPTH_TEST);
 glDepthMask(GL_FALSE);
 glDisable(GL_CULL_FACE);
 glDisable(GL_BLEND);
 glBindBuffer(GL_ARRAY_BUFFER, 0);
 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
 glEnableClientState(GL_VERTEX_ARRAY);
 glEnableClientState(GL_COLOR_ARRAY);
 glDisableClientState(GL_NORMAL_ARRAY);
 glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 glDisable(GL_TEXTURE_2D);
 if (quads) drawOverlay(overlay, quads * 6);
 glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 glEnable(GL_TEXTURE_2D);
 glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
 if (telemetryRows) {
  static OverlayVertex rows[6];
  quad(rows, TelemetryX, TelemetryY, 32 * 7, telemetryRows * 4, 255, 255, 255,
       0, 0, 1, telemetryRows / (float)TelemetryTexH);
  glBindTexture(GL_TEXTURE_2D, telemetryTexture);
  drawOverlay(rows, 6);
 }
 if (glyphQuads) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindTexture(GL_TEXTURE_2D, fontTexture);
  drawOverlay(glyphs, glyphQuads * 6);
  glDisable(GL_BLEND);
 }
 quads = glyphQuads = 0;
}

// Profiling metrics
static uint32_t lastFrameUs = 0;
static uint32_t avgFrameUs = 33333; // initial estimate ~30 FPS
static uint32_t lastInputUs, lastAudioUs, lastStreamUs, lastDrawUs, lastOverlayUs, lastTotalUs;
// Frame intervals bucketed by 60 Hz TE periods: 1, 2, 3, 4+ (cumulative, saturating).
static uint32_t vsyncBuckets[4];
static uint32_t pack16(uint32_t lo, uint32_t hi) {
 return (lo > 0xffff ? 0xffff : lo) | (hi > 0xffff ? 0xffff : hi) << 16;
}

extern "C" void gl_app_init() {
 nano_run_constructors();
 hb_touch_init();
 hh_bt_init();
}

extern "C" void gl_app_frame(int w, int h, uint32_t frame) {
 if (w <= 0 || h <= 0) return;
 uint32_t frameStart = hb_time_uptime_us();
 m8ProfPhase(M8P_FRAME);

 if (!ready) {
  if (!NanoInit(w, h)) { errors++; return; }
  createFontTexture();
  createTelemetryTexture();
  ready = true;
 }
#ifndef M8_NO_TELEMETRY
 uploadTelemetry();
#endif

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
 if (__builtin_expect(glGetError() != GL_NO_ERROR, 0)) errors++;
 lastStreamUs = nanoStreamUs;
 lastDrawUs = nanoDrawUs;

 loadedHeap = hb_os_heap_free();
 if (loadedHeap < heapLow) heapLow = loadedHeap;
 balanced = balanced && !stream.failed && !NanoFailures() && NanoPeak() < HeapBudget;

 // 4. 2D Overlay, FPS Counter & Profiling
 m8ProfPhase(M8P_OVERLAY);
 uint32_t tOverlay0 = hb_time_uptime_us();
 quads = glyphQuads = 0;

 // Compute FPS
 uint32_t dtFrame = (lastFrameUs > 0) ? (frameStart - lastFrameUs) : 33333;
 lastFrameUs = frameStart;
 if (dtFrame > 1000 && dtFrame < 1000000) {
  avgFrameUs = (avgFrameUs * 7 + dtFrame) / 8; // smooth 8-frame moving average
  unsigned periods = (dtFrame + 8333) / 16667;
  if (periods < 1) periods = 1;
  if (periods > 4) periods = 4;
  vsyncBuckets[periods - 1]++;
 }
 uint32_t fpsX10 = (avgFrameUs > 0) ? (10000000u / avgFrameUs) : 0;
 uint32_t fpsInt = fpsX10 / 10;
 uint32_t fpsFrac = fpsX10 % 10;

 // Machine-readable bit rows (rows 0-15 preserved from M7, rows 16-27 for M8 performance,
 // rows 28-35 for frame pacing; work/overlay values are from the previous frame)
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
  lastInputUs,
  m8Stats.texturePackedBytes,
  m8Stats.textureResidentBytes,
  m8Stats.textureUploads,
  m8Stats.textureFallbacks,
  m8Stats.drawCalls,
  m8Stats.vertices,
  m8Stats.indices,
  lastTotalUs,
  dtFrame,
  pack16(lastOverlayUs, nanoEndUs),
  pack16(vsyncBuckets[0], vsyncBuckets[1]),
  pack16(vsyncBuckets[2], vsyncBuckets[3]),
  pack16(nanoBenchElementsX10, nanoBenchArraysX10),
  pack16(nanoClearUs, nanoBeginUs),
  nanoWorldUs
 };

 // Draw On-Screen FPS Counter (top-left in landscape, with background plate)
 static char fpsStr[16];
 static uint32_t fpsTextUs;
 if (!fpsStr[0] || frameStart - fpsTextUs >= 250000) {
  snprintf(fpsStr, sizeof(fpsStr), "FPS %u.%u", (unsigned)fpsInt, (unsigned)fpsFrac);
  fpsTextUs = frameStart;
 }
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

 // Machine-readable telemetry: one quad over the bit texture (uploaded next frame).
#ifndef M8_NO_TELEMETRY
 setTelemetry(values, sizeof(values) / sizeof(values[0]));
#else
 (void)values;
#endif

 flushOverlay2D();
 lastOverlayUs = hb_time_uptime_us() - tOverlay0;
 uint32_t totalFrameUs = hb_time_uptime_us() - frameStart;
 lastTotalUs = totalFrameUs;
 m8ProfPhase(M8P_OUTSIDE);

 // Periodic profiling log every 30 frames
 if (frame % 30 == 0) {
  fprintf(stderr, "[M8-PROFILE] frame=%u fps=%u.%u total=%u us (draw=%u stream=%u audio=%u input=%u overlay=%u)\n",
          (unsigned)frame, (unsigned)fpsInt, (unsigned)fpsFrac, (unsigned)totalFrameUs,
          (unsigned)lastDrawUs, (unsigned)lastStreamUs, (unsigned)lastAudioUs,
          (unsigned)lastInputUs, (unsigned)lastOverlayUs);
 }
}
