#pragma once
#include <stddef.h>
bool NanoInit(int width,int height);
bool NanoFrame(unsigned tick);
bool NanoUnload();
bool NanoExit();
size_t NanoLive();
size_t NanoPeak();
size_t NanoBaseline();
unsigned NanoCycles();
