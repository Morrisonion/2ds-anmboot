#include "minilib.h"
void *memcpy(void *dst, const void *src, size_t n) {
    u8 *d=(u8*)dst; const u8 *s=(const u8*)src;
    while(n--) *d++=*s++;
    return dst;
}
void *memset(void *dst, int c, size_t n) {
    u8 *d=(u8*)dst; while(n--) *d++=(u8)c; return dst;
}
int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x=(const u8*)a,*y=(const u8*)b;
    while(n--) { if(*x!=*y) return (int)*x-(int)*y; x++; y++; }
    return 0;
}
size_t strlen(const char *s) { size_t n=0; while(s[n]) n++; return n; }

/* Freestanding ARM EABI unsigned division helper used by the software renderer. */
u32 __aeabi_uidiv(u32 n, u32 d) {
    if(d==0) return 0xFFFFFFFFu;
    u32 q=0, bit=1;
    while((d < n) && !(d & 0x80000000u)) { d <<= 1; bit <<= 1; }
    while(bit) { if(n >= d) { n -= d; q |= bit; } d >>= 1; bit >>= 1; }
    return q;
}
