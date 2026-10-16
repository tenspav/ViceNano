#include "audio.h"
#include "generated/audio.h"
#include <string.h>
#ifndef AUDIO_HOST
extern "C" {
#include "hb_sdk.h"
#include "hb_heap.h"
}
#endif
cDMAudio DMAudio;
AudioStats audioStats;
static bool initialised,radio;
static unsigned effectsGain=100,musicGain=65,entityMask;
struct Voice {const int16_t *data;unsigned length,pos,fraction,gain,pan;int entity;};
static Voice voices[8];
static unsigned radioPos,radioFrac,cacheBlock=~0u;
static int16_t radioCache[256];
static const int steps[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static int16_t radioSample(){
 unsigned block=radioPos/256;
 if(block!=cacheBlock){
  // Bounded read-at from immutable backing store: one 132-byte compressed block.
  // No full-track PCM allocation, no decoder state shared between seeks.
  uint8_t packed[132];memcpy(packed,radioData+block*132,132);audioStats.radioReads++;
  int pred=int16_t(packed[0]|unsigned(packed[1])<<8),idx=packed[2];
  static const int adjust[]={-1,-1,-1,-1,2,4,6,8};
  for(unsigned i=0;i<256;i++){
   unsigned code=(packed[4+i/2]>>((i&1)*4))&15;int step=steps[idx];
   int delta=(step>>3)+((code&4)?step:0)+((code&2)?step>>1:0)+((code&1)?step>>2:0);
   pred=clamp(pred+((code&8)?-delta:delta),-32768,32767);idx=clamp(idx+adjust[code&7],0,88);radioCache[i]=pred;
  }cacheBlock=block;
 }
 return radioCache[radioPos%256];
}
void AudioMix(int16_t *out,unsigned frames){
 unsigned active=0;for(unsigned v=0;v<8;v++)active+=voices[v].data!=0;
 if(active>audioStats.voicesPeak)audioStats.voicesPeak=active;
 for(unsigned i=0;i<frames;i++){
  int left=0,right=0;
  if(radio){int s=radioSample()*int(musicGain)/128;left=right=s;
   radioFrac+=8000;if(radioFrac>=44100){radioFrac-=44100;if(++radioPos==radioSamples){radio=false;audioStats.radioEnds++;}}
  }
  if(active){
   for(unsigned v=0;v<8;v++){
    Voice &a=voices[v];if(!a.data)continue;
    int s=int(a.data[a.pos])*int(a.gain)*int(effectsGain)/16384;
    left+=s*int(127-a.pan)/127;right+=s*int(a.pan)/127;
    a.fraction+=8000;if(a.fraction>=44100){a.fraction-=44100;if(++a.pos>=a.length)a.data=0;}
   }
  }
  out[2*i]=clamp(left,-32768,32767);out[2*i+1]=clamp(right,-32768,32767);
 }
 audioStats.frames+=frames;audioStats.blocks++;
}
#ifndef AUDIO_HOST
// NanoEmu/RetailOS snapshot-specific output adapter. Own channel 2 only while
// active; preserve/restore the OS ring. Completion IRQ remains handled by RetailOS.
static const unsigned Frames=8192,Count=4;
static int16_t *ring;
static uint32_t *desc;
static uint32_t saved[6],savedRate;
static unsigned playing;
static volatile uint32_t *const channel=(volatile uint32_t*)0x38200140;
static volatile uint32_t *const divisor=(volatile uint32_t*)0x3ca00040;
static void clean(void *p,unsigned n){
 register uint32_t r12 __asm__("r12")=7;
 register uint32_t r0 __asm__("r0")=(uint32_t)p;
 register uint32_t r1 __asm__("r1")=n;
 __asm__ volatile("svc #70":"+r"(r12),"+r"(r0),"+r"(r1)::"memory");
}
static void fill(unsigned slot){
 uint32_t start=hb_time_uptime_us();AudioMix(ring+slot*Frames*2,Frames);
 unsigned elapsed=hb_time_uptime_us()-start;audioStats.mixUs+=elapsed;
 if(elapsed>audioStats.maxMixUs)audioStats.maxMixUs=elapsed;
 clean(ring+slot*Frames*2,Frames*4);
}
static bool outputStart(){
 ring=(int16_t*)hb_os_alloc(Count*Frames*4+Count*20+32);if(!ring)return false;
 audioStats.live=Count*Frames*4+Count*20+32;if(audioStats.live>audioStats.peak)audioStats.peak=audioStats.live;
 desc=(uint32_t*)((uintptr_t(ring)+Count*Frames*4+31)&~uintptr_t(31));
 for(unsigned i=0;i<6;i++)saved[i]=channel[i];
 savedRate=*divisor;
 channel[4]=saved[4]&~1u;
 for(unsigned i=0;i<Count;i++){
  fill(i);uint32_t *d=desc+i*5;d[0]=(uint32_t)(ring+i*Frames*2);d[1]=0x3ca00010;
  d[2]=(uint32_t)(desc+((i+1)%Count)*5);d[3]=0x84249000;d[4]=Frames*2;
 }
 clean(desc,Count*20);*divisor=0x110;
 channel[0]=desc[0];channel[1]=desc[1];channel[2]=desc[2];channel[3]=desc[3];channel[5]=desc[4];playing=0;
 channel[4]=0x8b01;return true;
}
static void outputStop(){
 channel[4]&=~1u;
 for(unsigned i=0;i<6;i++)if(i!=4)channel[i]=saved[i];
 *divisor=savedRate;channel[4]=saved[4];
 unsigned before=hb_os_heap_free();hb_os_free(ring);
 if(hb_os_heap_free()-before<audioStats.live)audioStats.failures++;
 ring=0;desc=0;audioStats.live=0;
}
#endif
void cDMAudio::Initialise(){if(initialised)return;
 memset(voices,0,sizeof(voices));radio=false;entityMask=0;
#ifndef AUDIO_HOST
 if(!outputStart()){audioStats.failures++;return;}
#endif
 initialised=true;audioStats.starts++;
}
void cDMAudio::Terminate(){if(!initialised)return;
#ifndef AUDIO_HOST
 outputStop();
#endif
 initialised=false;radio=false;memset(voices,0,sizeof(voices));entityMask=0;audioStats.stops++;
}
uint8_t cDMAudio::IsAudioInitialised(){return initialised;}
void cDMAudio::Service(){
#ifndef AUDIO_HOST
 if(!initialised)return;
 unsigned addr=channel[0],current=Count;
 for(unsigned i=0;i<Count;i++)if(addr==(uint32_t)(ring+i*Frames*2))current=i;
 if(current==Count){audioStats.failures++;Terminate();return;}
 // Refill only completed buffers; no writes to the DMA-owned buffer.
 while(playing!=current){fill(playing);playing=(playing+1)%Count;}
#endif
}
int32_t cDMAudio::CreateEntity(eAudioType,void*){for(unsigned i=0;i<16;i++)if(!(entityMask&(1u<<i))){entityMask|=1u<<i;return i;}return -1;}
void cDMAudio::DestroyEntity(int32_t e){if(e<0||e>=16)return;entityMask&=~(1u<<e);for(unsigned i=0;i<8;i++)if(voices[i].entity==e)voices[i].data=0;}
void cDMAudio::PlayOneShot(int32_t entity,uint16_t sound,float volume){
 if(!initialised||entity<0||entity>=16||!(entityMask&(1u<<entity))||sound>=3)return;
 for(unsigned i=0;i<8;i++)if(!voices[i].data){
  static const int16_t *const data[]={sfx0,sfx1,sfx2};
  static const unsigned lengths[]={sizeof(sfx0)/2,sizeof(sfx1)/2,sizeof(sfx2)/2};
  Voice &v=voices[i];v.data=data[sound];v.length=lengths[sound];v.pos=v.fraction=0;v.gain=clamp(int(volume*127),0,127);v.pan=sound==0?20:sound==1?64:107;v.entity=entity;return;
 }
}
void cDMAudio::PlayFrontEndSound(uint16_t s,uint32_t vol){if(!(entityMask&1))entityMask|=1;PlayOneShot(0,s,float(vol>127?127:vol)/127);}
void cDMAudio::PlayFrontEndTrack(uint32_t track,uint8_t){SetRadioChannel(track,0);}
void cDMAudio::StopFrontEndTrack(){radio=false;}
void cDMAudio::SetRadioChannel(uint32_t station,int32_t ms){
 if(!initialised)return;
 if(station!=0){radio=false;return;}
 radioPos=ms<0?0:ms>=60000?radioSamples:unsigned(ms)*8;radioFrac=0;cacheBlock=~0u;radio=radioPos<radioSamples;
}
void cDMAudio::SetEffectsMasterVolume(uint8_t v){effectsGain=v>127?127:v;}
void cDMAudio::SetMusicMasterVolume(uint8_t v){musicGain=v>127?127:v;}
void AudioTestTick(){
 static unsigned phase,baseFrames,lastTrigger,cycleBase;
 if(!phase){DMAudio.Initialise();if(!DMAudio.IsAudioInitialised())return;
  DMAudio.PlayFrontEndTrack(0,1);baseFrames=audioStats.frames;lastTrigger=~0u;phase=1;
 }
 DMAudio.Service();
 unsigned elapsed=audioStats.frames-baseFrames;
 unsigned trigger=elapsed/44100;
 if(trigger!=lastTrigger){for(unsigned i=0;i<3;i++)DMAudio.PlayFrontEndSound(i,90);lastTrigger=trigger;}
 // Loop the minute-long track cleanly without reallocating DMA ring buffers.
 if(phase==1 && elapsed>=65*44100){
  DMAudio.SetRadioChannel(0,0);
  baseFrames=audioStats.frames;
  audioStats.cycles++;
 }
}
