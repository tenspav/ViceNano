#include "audio.h"
#include <stdio.h>
static int16_t out[8192*2];
int main(int argc,char**){
 DMAudio.Initialise();DMAudio.PlayFrontEndTrack(0,1);
 if(argc==1){for(unsigned i=0;i<3;i++)DMAudio.PlayFrontEndSound(i,90);AudioMix(out,8192);return fwrite(out,1,sizeof(out),stdout)!=sizeof(out);}
 for(unsigned frames=0;frames<60*44100;){unsigned n=60*44100-frames;if(n>8192)n=8192;AudioMix(out,n);if(fwrite(out,4,n,stdout)!=n)return 1;frames+=n;}return 0;
}
