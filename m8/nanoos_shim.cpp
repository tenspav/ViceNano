#include "nanoos_shim.h"
#include "nano.h"
#include "input.h"
extern "C" {
#include "lcd.h"
#include <timer.h>
}
static unsigned frame;
static bool active;
static unsigned renderMode;
uint32_t m8_time_us(void) { return USEC; }
void m8_title(unsigned tick) {
 lcd_clear(0xff0d1440);
 lcd_center(48, "ViceNano", WHITE);
 lcd_center(80, "M8 / NanoOS", WHITE);
 lcd_center(120, "Loading", WHITE);
 for (int y=148;y<154;++y)
  for (int x=80;x<352;++x) lcd_pixel(x,y,0xff26365c);
 for (int y=148;y<154;++y)
  for (unsigned x=0;x<((tick/2)%272);++x) lcd_pixel(80+x,y,0xffff8800);
 lcd_center(184, "Title screen port", WHITE);
 lcd_center(208, "Home to return", WHITE);
 lcd_present();
}
void m8_app_enter(void) {
 frame=0;renderMode=0;
 active=NanoInit(LCD_WIDTH,LCD_HEIGHT);
 if(active) NanoFrame(frame++);
}
void m8_app_tick(unsigned keys) {
 // NanoOS exposes button press edges, not touch contacts or a HID transport.
 pad.OldState=pad.NewState;
 pad.NewState={};
 pad.NewState.LeftStickX=(keys&2)?-32767:(keys&4)?32767:0;
 if(keys&2)renderMode=(renderMode+3)%4;
 if(keys&4)renderMode=(renderMode+1)%4;
 m8_render_mode(renderMode);
 if(active && lcd_ready()) NanoFrame(frame++);
}
void m8_app_exit(void) { NanoExit(); active=false; }
