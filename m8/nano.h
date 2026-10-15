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
extern uint32_t nanoStreamUs, nanoDrawUs;
extern float playerX, playerZ, cameraYaw, cameraPitch;
