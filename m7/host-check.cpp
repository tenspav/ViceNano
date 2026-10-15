#include "input.h"
#include <cassert>
#include <cstdio>
int main(){
 Contact c[3]={{0,60,120},{1,390,145},{2,200,100}};
 TouchUpdate(c,3);pad.Update(false,0);assert(pad.NewState.Cross==255);
 c[0].x=102;c[2].x=208;TouchUpdate(c,3);pad.Update(false,0);
 assert(pad.NewState.LeftStickX==32760&&pad.NewState.RightStickX==14400);
 TouchUpdate(0,0);pad.Update(false,0);assert(!pad.NewState.Cross&&!pad.NewState.LeftStickX);
 assert(pad.OldState.Cross==255&&pad.OldState.LeftStickX==32760);
 Contact diagonal={0,60,100};TouchUpdate(&diagonal,1);diagonal.x=102;diagonal.y=142;
 TouchUpdate(&diagonal,1);pad.Update(false,0);
 int x=pad.NewState.LeftStickX,y=pad.NewState.LeftStickY;
 assert(x>23000&&y<-23000&&x*x+y*y<=32767*32767);
 TouchUpdate(0,0);
 hid_pad p={};p.hat=1;p.axis[0]=32767;p.axis[1]=-32767;p.axis[3]=-16384;p.buttons=0xfff;
 pad.Update(true,&p);assert(pad.NewState.LeftStickY==32767&&pad.NewState.RightStickX==-16384);
 assert(pad.NewState.Cross==255&&pad.NewState.Select==255&&pad.NewState.DPadUp==255&&pad.NewState.DPadRight==255);
 pad.Update(true,0);assert(!pad.NewState.Cross&&!pad.NewState.LeftStickX);
 for(int b=0;b<10;b++){Contact t={0,buttons[b].x+1,buttons[b].y+1};TouchUpdate(&t,1);pad.Update(false,0);assert(reinterpret_cast<int16_t*>(&pad.NewState)[buttons[b].field]==255);TouchUpdate(0,0);}
 for(int i=0;i<1000;i++){c[0].x=60;TouchUpdate(c,1);c[0].x=100;TouchUpdate(c,1);TouchUpdate(0,0);}
 puts("PASS: simultaneous stick/button/camera, release, all buttons, HID axes/buttons/hat, disconnect, 3000 touch updates");
}
