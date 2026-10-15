#include "input.h"
#include <string.h>
#include <stddef.h>
CPad pad;unsigned inputEvents;
static CControllerState touch;
static struct Finger {bool active;int x,y,lastx,lasty,role;} fingers[8];
#define B(x,y,w,h,f) {x,y,w,h,int(offsetof(CControllerState,f)/sizeof(int16_t))}
const Button buttons[10]={B(0,0,40,40,Start),B(45,0,50,40,RightShock),B(305,0,27,40,LeftShoulder2),B(332,0,28,40,RightShoulder2),B(385,0,45,45,RightShoulder1),B(365,55,55,55,Triangle),B(310,105,55,55,Circle),B(310,165,55,55,Square),B(365,120,60,65,Cross),B(10,190,45,45,LeftShock)};
static int16_t clamp(int v){return v<-32767?-32767:v>32767?32767:v;}
void TouchUpdate(const Contact *c,int n){
 bool seen[8]={};memset(&touch,0,sizeof touch);
 for(int i=0;i<n;i++){
  if(c[i].id<0||c[i].id>=8)continue;
  Finger &f=fingers[c[i].id];seen[c[i].id]=true;
  if(!f.active){f.x=f.lastx=c[i].x;f.y=f.lasty=c[i].y;f.role=-1;
   for(int b=0;b<10;b++)if(c[i].x>=buttons[b].x&&c[i].x<buttons[b].x+buttons[b].w&&c[i].y>=buttons[b].y&&c[i].y<buttons[b].y+buttons[b].h){f.role=b;break;}
   if(f.role<0&&c[i].x>=10&&c[i].x<=130&&c[i].y>=70)f.role=10;
   if(f.role<0&&c[i].x>=140&&c[i].x<=310&&c[i].y>=40&&c[i].y<=200)f.role=11;
   inputEvents++;
  }else if(f.lastx!=c[i].x||f.lasty!=c[i].y)inputEvents++;
  f.active=true;
  if(f.role>=0&&f.role<10)reinterpret_cast<int16_t*>(&touch)[buttons[f.role].field]=255;
  if(f.role==10){touch.LeftStickX=clamp((c[i].x-f.x)*780);touch.LeftStickY=clamp(-(c[i].y-f.y)*780);}
  if(f.role==11){touch.RightStickX=clamp((c[i].x-f.lastx)*1800);touch.RightStickY=clamp(-(c[i].y-f.lasty)*1800);}
  f.lastx=c[i].x;f.lasty=c[i].y;
 }
 for(int i=0;i<8;i++)if(fingers[i].active&&!seen[i]){fingers[i].active=false;inputEvents++;}
}
void CPad::Update(bool connected,const hid_pad *p){
 OldState=NewState;NewState=touch;
 if(!connected)return;
 memset(&NewState,0,sizeof NewState);if(!p)return;
 NewState.LeftStickX=p->axis[0];NewState.LeftStickY=clamp(-int(p->axis[1]));
 NewState.RightStickX=p->axis[3];NewState.RightStickY=clamp(-int(p->axis[4]));
 int16_t *fields=&NewState.Cross;
 for(int i=0;i<12;i++)fields[i]=(p->buttons&(1u<<i))?255:0;
 if(p->axis[2]>8000)NewState.LeftShoulder2=255;if(p->axis[5]>8000)NewState.RightShoulder2=255;
 int h=p->hat;NewState.DPadUp=(h==7||h==0||h==1)?255:0;NewState.DPadRight=(h>=1&&h<=3)?255:0;
 NewState.DPadDown=(h>=3&&h<=5)?255:0;NewState.DPadLeft=(h>=5&&h<=7)?255:0;
}
