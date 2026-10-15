#pragma once
#include "model.h"
const unsigned StreamSlots=2;
const unsigned HeapBudget=18*1024*1024;
struct StreamState {
 int resident[StreamSlots];
 unsigned age[StreamSlots],clock,loads,evictions,hits,active;
 bool failed;
};
extern StreamState stream;
void StreamReset();
bool StreamRequest(const NanPack *packs,unsigned count,float cameraX,float cameraZ);
void StreamClear();
void StreamRender();
