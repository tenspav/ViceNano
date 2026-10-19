#include <stddef.h>
/* Compiler-generated structure copies/clears; no hosted libc or OS calls. */
void *memset(void *dst, int value, size_t n) {
 unsigned char *d = dst;
 for (size_t i=0;i<n;++i) d[i]=(unsigned char)value;
 return dst;
}
void *memcpy(void *dst, const void *src, size_t n) {
 unsigned char *d=dst; const unsigned char *s=src;
 for (size_t i=0;i<n;++i) d[i]=s[i];
 return dst;
}
/* Integer division for the -lgcc callers (the kernel is at its 64 KiB cap):
 * libgcc's unrolled ARM routines take 1.3 KB, this shift-subtract loop is a
 * tenth of that. No caller is hot (setup, text, the camera); a zero divisor
 * gives an all-ones quotient instead of __aeabi_idiv0. The divmod forms
 * return the quotient in r0 and the remainder in r1 (a 64-bit return). */
typedef unsigned long long u64;
u64 __aeabi_uidivmod(unsigned n, unsigned d) {
 unsigned q=0,r=0;
 for (int i=31;i>=0;--i) { r=r<<1|(n>>i&1); if (r>=d) { r-=d; q|=1u<<i; } }
 return (u64)r<<32|q;
}
unsigned __aeabi_uidiv(unsigned n, unsigned d) { return (unsigned)__aeabi_uidivmod(n,d); }
u64 __aeabi_idivmod(int n, int d) {
 u64 x=__aeabi_uidivmod(n<0?0u-(unsigned)n:(unsigned)n, d<0?0u-(unsigned)d:(unsigned)d);
 unsigned q=(unsigned)x,r=(unsigned)(x>>32);
 if ((n^d)<0) q=0u-q;
 if (n<0) r=0u-r;
 return (u64)r<<32|q;
}
int __aeabi_idiv(int n, int d) { return (int)__aeabi_idivmod(n,d); }
