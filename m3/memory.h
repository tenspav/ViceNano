#pragma once
#include "../vendor/librw/rw.h"
#include <stdlib.h>
#include <string.h>
static size_t rwLive,rwPeak;
static void *trackedAlloc(size_t n,rw::uint32){
 size_t *p=(size_t*)malloc(n+8);if(!p)return nullptr;*p=n;rwLive+=n;if(rwLive>rwPeak)rwPeak=rwLive;return (char*)p+8;
}
static void trackedFree(void *p){if(!p)return;size_t *h=(size_t*)((char*)p-8);rwLive-=*h;free(h);}
static void *trackedResize(void *p,size_t n,rw::uint32 hint){
 void *q=trackedAlloc(n,hint);if(p && q){size_t old=*(size_t*)((char*)p-8);memcpy(q,p,old<n?old:n);trackedFree(p);}return q;
}
static rw::MemoryFunctions trackedMemory={trackedAlloc,trackedResize,trackedFree,trackedAlloc,trackedResize};
