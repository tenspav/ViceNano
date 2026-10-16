/* Local compressed-kernel wrapper. Uses NanoOS's existing shutdown/handoff.
 * Decoder is bounded by compressed input, 1 MiB output and match history. */
#include <stdint.h>
#include "shutdown.h"
#include "handoff.h"
static uint8_t staging[1024*1024] __attribute__((aligned(32)));
static uint32_t decode(void){
 const uint8_t *p=default_payload_bin,*end=default_payload_bin_end;
 if(end-p<4)return 0;
 uint32_t size=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
 p+=4;
 if(!size||size>sizeof(staging))return 0;
 uint32_t at=0;
 while(at<size){
  if(p==end)return 0;
  unsigned flags=*p++;
  for(unsigned bit=0;bit<8&&at<size;bit++){
   if(flags&(1u<<bit)){if(p==end)return 0;staging[at++]=*p++;}
   else {
    if(end-p<2)return 0;
    unsigned token=p[0]|(unsigned)p[1]<<8;p+=2;
    unsigned distance=(token>>4)+1,n=(token&15)+3;
    if(n==18){if(p==end)return 0;n+=*p++;}
    if(distance>at||n>size-at)return 0;
    while(n--){staging[at]=staging[at-distance];++at;}
   }
  }
 }
 if(end-p>3)return 0;
 while(p<end)if(*p++)return 0;
 return size;
}
__attribute__((section(".text.entry"),used,noinline))
void payload_entry(void){
 uint32_t size=decode();
 if(!size)return;
 hw_shutdown_all();
 handoff_to_payload(staging,size);
}
