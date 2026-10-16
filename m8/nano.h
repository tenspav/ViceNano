#pragma once
#include <stddef.h>
#include <stdint.h>
bool NanoInit(int width,int height);
bool NanoFrame(unsigned tick);
bool NanoUnload();
bool NanoExit();
size_t NanoLive();
size_t NanoPeak();
size_t NanoBaseline();
unsigned NanoCycles();
unsigned NanoFailures();
void NanoSetHardwareUnlock(bool enable);
extern uint32_t nanoStreamUs, nanoDrawUs, nanoEndUs, nanoClearUs, nanoBeginUs, nanoWorldUs;
// One-shot draw-path micro-benchmark: microseconds per 1-triangle draw, x10.
extern uint32_t nanoBenchElementsX10, nanoBenchArraysX10;
// Frame phase for NanoEmu's guest profiler (NANO7_PROF); no-op unless -DM8_PROFILE.
enum{M8P_OUTSIDE,M8P_FRAME,M8P_STREAM,M8P_BEGIN,M8P_WORLD,M8P_END,M8P_OVERLAY};
#ifdef M8_PROFILE
void m8ProfPhase(uint32_t phase);
#else
static inline void m8ProfPhase(uint32_t){}
#endif
extern float playerX, playerZ, cameraYaw, cameraPitch;
