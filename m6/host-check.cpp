#include "audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int16_t samples[1024*2],other[1024*2];
static bool nonzero(){for(unsigned i=0;i<2048;i++)if(samples[i])return true;return false;}
int main(){
 DMAudio.Initialise();assert(DMAudio.IsAudioInitialised());
 AudioMix(samples,1024);assert(!nonzero());
 int entity=DMAudio.CreateEntity(AUDIOTYPE_PHYSICAL,0);assert(entity>=0);
 for(unsigned i=0;i<8;i++)DMAudio.PlayOneShot(entity,i%3,1);
 AudioMix(samples,1024);assert(nonzero());assert(audioStats.voicesPeak==8);
 DMAudio.DestroyEntity(entity);AudioMix(samples,1024);assert(!nonzero());
 // Seek must rebuild an independent IMA block and replay bit-for-bit.
 DMAudio.SetRadioChannel(0,12345);AudioMix(samples,1024);assert(nonzero());memcpy(other,samples,sizeof(samples));
 DMAudio.StopFrontEndTrack();AudioMix(samples,1024);assert(!nonzero());
 DMAudio.SetRadioChannel(0,12345);AudioMix(samples,1024);assert(!memcmp(other,samples,sizeof(samples)));
 DMAudio.SetRadioChannel(0,59980);AudioMix(samples,1024);assert(audioStats.radioEnds==1);AudioMix(samples,1024);assert(!nonzero());
 DMAudio.SetRadioChannel(0,60000);AudioMix(samples,1024);assert(!nonzero());
 DMAudio.SetRadioChannel(99,0);AudioMix(samples,1024);assert(!nonzero());
 DMAudio.Terminate();DMAudio.Terminate();
 for(unsigned i=0;i<1000;i++){DMAudio.Initialise();DMAudio.PlayFrontEndTrack(0,1);DMAudio.PlayFrontEndSound(i%3,100);AudioMix(samples,1024);assert(nonzero());DMAudio.StopFrontEndTrack();DMAudio.Terminate();AudioMix(samples,1024);assert(!nonzero());}
 printf("PASS: 8 concurrent voices, entity teardown, radio seek/replay/stop/EOF, invalid station, 1000 lifecycle cycles; reads=%u blocks=%u\n",audioStats.radioReads,audioStats.blocks);
}
