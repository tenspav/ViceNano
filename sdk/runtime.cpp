/* Minimal Nano runtime for the standalone RW viewer. Host converter uses libc. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "hb_heap.h"
void nano_run_constructors(){extern uintptr_t __init_array_start[],__init_array_end[];for(auto p=__init_array_start;p<__init_array_end;p++)((void(*)())((uintptr_t)p+*p))();}
/* Each allocation has size metadata for checked realloc; OS allocator is nothrow. */
void *malloc(size_t n){if(n>0x1000000)return nullptr;size_t *p=(size_t*)hb_os_alloc(n+8);if(!p)return nullptr;p[0]=n;return (char*)p+8;}
void free(void *p){if(p)hb_os_free((char*)p-8);}
void *realloc(void *p,size_t n){if(!p)return malloc(n);if(!n){free(p);return nullptr;}void *q=malloc(n);if(!q)return nullptr;size_t old=*(size_t*)((char*)p-8);memcpy(q,p,old<n?old:n);free(p);return q;}
void *calloc(size_t a,size_t b){if(b && a>0x1000000/b)return nullptr;void *p=malloc(a*b);if(p)memset(p,0,a*b);return p;}
/* Nano image IO is intentionally unavailable here; converted assets are embedded.
 * Do not silently pretend these stubs can read the user's IMG on device. */
int _open(const char*,int,...){return -1;}
void *_sbrk(ptrdiff_t){return (void*)-1;}
void _fini(){}
void __libc_fini(){}
int _close(int){return -1;}int _fstat(int,void*){return -1;}int _isatty(int){return 0;}
int _lseek(int,int,int){return -1;}int _read(int,void*,int){return -1;}
int _write(int,const void*,int){return -1;}int _getpid(){return 1;}int _kill(int,int){return -1;}
void _exit(int){for(;;){}}
}
void *operator new(size_t n){return malloc(n);}
void *operator new[](size_t n){return malloc(n);}
void operator delete(void *p) noexcept {free(p);}
void operator delete[](void *p) noexcept {free(p);}
void operator delete(void *p,size_t) noexcept {free(p);}
void operator delete[](void *p,size_t) noexcept {free(p);}
