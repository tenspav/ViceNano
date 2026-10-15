#include "nano.h"
#include "stream.h"
#include "generated/world.h"
#include "../vendor/librw/rw.h"
#include <assert.h>
#include <stdio.h>
extern unsigned mock_uploads,mock_deletes,mock_draws;
int main(){
 assert(!NanoInit(0,432));assert(!NanoFrame(0));assert(NanoExit());
 assert(NanoInit(240,432));size_t baseline=NanoBaseline();
 // Check an actual cache hit changes the LRU victim, then request a third cell.
 assert(StreamRequest(worldPacks,3,worldPacks[0].x,worldPacks[0].z-140));
 assert(StreamRequest(worldPacks,3,worldPacks[1].x,worldPacks[1].z-140));
 assert(StreamRequest(worldPacks,3,worldPacks[0].x,worldPacks[0].z-140));
 assert(StreamRequest(worldPacks,3,worldPacks[2].x,worldPacks[2].z-140));
 assert(stream.resident[0]==0 && stream.resident[1]==2 && stream.evictions==1 && stream.hits==1);
 assert(NanoUnload());assert(NanoLive()==baseline);
 NanPack oversized=worldPacks[0];oversized.reservation=HeapBudget;
 unsigned before=stream.loads;
 assert(!StreamRequest(&oversized,1,oversized.x,oversized.z-140));
 assert(stream.loads==before && NanoLive()==baseline);
 StreamReset();
 assert(StreamRequest(worldPacks,3,-10000,-10000));assert(stream.loads==0);
 size_t balances[3]={};unsigned totalLoads=stream.loads;
 for(unsigned traversal=0;traversal<30;traversal++){
  for(unsigned tick=0;tick<108;tick++){
   assert(NanoFrame(tick));assert(NanoFailures()==0);assert(NanoLive()<HeapBudget);
   // Same warmed two-block state must return exactly the same application bytes.
   if(tick==24 || tick==60 || tick==96){unsigned i=tick/36;if(traversal==0)balances[i]=NanoLive();else assert(balances[i]==NanoLive());}
  }
  assert(stream.evictions>0);totalLoads=stream.loads;
  assert(NanoUnload());assert(NanoLive()==baseline);assert(NanoUnload());
 }
 assert(NanoExit());assert(NanoLive()==0);assert(NanoExit());
 assert(mock_uploads==mock_deletes && mock_draws>100);
 printf("PASS: LRU victim/hit; 30 traversals, %u loads, final live=%zu peak=%zu baseline=%zu uploads=%u deletes=%u draws=%u failures=%u budget=%u\n",totalLoads,NanoLive(),NanoPeak(),baseline,mock_uploads,mock_deletes,mock_draws,NanoFailures(),HeapBudget);
}
