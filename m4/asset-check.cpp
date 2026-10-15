#include "nano.h"
#include "../vendor/librw/rw.h"
#include <assert.h>
#include <stdio.h>
extern unsigned mock_uploads,mock_deletes,mock_draws;
int main(){
 assert(!NanoInit(0,432));assert(!NanoFrame(0));assert(NanoExit());
 for(unsigned cycle=0;cycle<20;cycle++){
  assert(NanoInit(240,432));assert(!NanoInit(240,432));size_t baseline=NanoBaseline();
  for(unsigned i=0;i<5;i++){
   assert(NanoFrame(i*37));assert(NanoLive()>baseline);assert(NanoUnload());assert(NanoLive()==baseline);assert(NanoUnload());
  }
  assert(NanoExit());assert(NanoLive()==0);assert(NanoExit());
 }
 assert(mock_uploads>=100 && mock_uploads==mock_deletes && mock_draws>=100);
 printf("PASS: 20 complete init/exit cycles; 100 multi-part load/render/unload cycles; live=%zu peak=%zu uploads=%u deletes=%u draws=%u\n",NanoLive(),NanoPeak(),mock_uploads,mock_deletes,mock_draws);
}
