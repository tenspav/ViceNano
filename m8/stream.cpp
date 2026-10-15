#include "stream.h"
#include <math.h>
StreamState stream;
void StreamReset(){stream={};for(unsigned i=0;i<StreamSlots;i++)stream.resident[i]=-1;}
void StreamClear(){
 for(unsigned i=0;i<StreamSlots;i++)if(stream.resident[i]>=0){unloadBlock(i);stream.resident[i]=-1;stream.evictions++;}
}
bool StreamRequest(const NanPack *packs,unsigned count,float cameraX,float cameraZ){
 // Predict the visible cell 140 world units ahead of the moving camera.
 int bx=(int)floorf(cameraX/100),bz=(int)floorf((cameraZ+140)/100);
 unsigned wanted=count;
 for(unsigned i=0;i<count;i++)if(packs[i].blockX==bx&&packs[i].blockZ==bz){wanted=i;break;}
 if(wanted==count)return true; // Empty cell: preserve the bounded warm cache.
 stream.active=wanted;stream.clock++;
 for(unsigned i=0;i<StreamSlots;i++)if(stream.resident[i]==(int)wanted){stream.age[i]=stream.clock;stream.hits++;return true;}
 unsigned victim=0;
 for(unsigned i=0;i<StreamSlots;i++){
  if(stream.resident[i]<0){victim=i;break;}
  if(stream.age[i]<stream.age[victim])victim=i;
 }
 unsigned reserved=packs[wanted].reservation;
 for(unsigned i=0;i<StreamSlots;i++)if(i!=victim && stream.resident[i]>=0)reserved+=packs[stream.resident[i]].reservation;
 // 2 MiB covers executable slot, stacks/display allowance and engine baseline.
 if(reserved>HeapBudget-2*1024*1024){stream.failed=true;return false;}
 if(stream.resident[victim]>=0){unloadBlock(victim);stream.resident[victim]=-1;stream.evictions++;}
 if(!loadBlock(victim,packs[wanted])){stream.failed=true;return false;}
 stream.resident[victim]=wanted;stream.age[victim]=stream.clock;stream.loads++;return true;
}
void StreamRender(){for(unsigned i=0;i<StreamSlots;i++)if(stream.resident[i]>=0)renderBlock(i);}
