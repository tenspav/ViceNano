#pragma once
#include <stdint.h>
#include "hid_parse.h"
// Reduced CPad equivalent; same field names and pressure/axis semantics.
struct CControllerState {
 int16_t LeftStickX,LeftStickY,RightStickX,RightStickY;
 int16_t Cross,Circle,Square,Triangle,LeftShoulder1,RightShoulder1,LeftShoulder2,RightShoulder2;
 int16_t LeftShock,RightShock,Start,Select,DPadUp,DPadDown,DPadLeft,DPadRight;
};
struct CPad { CControllerState NewState,OldState; void Update(bool connected,const hid_pad *p); };
struct Contact {int id,x,y;};
struct Button {int x,y,w,h;int field;};
extern const Button buttons[10];
extern CPad pad;
extern unsigned inputEvents;
void TouchUpdate(const Contact *c,int n);
