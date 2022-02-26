#ifndef VXWM_UTIL_H
#define VXWM_UTIL_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>

#define VXWM_LOG_FILE       stdout
#define VXWM_ERR_FILE       stderr

#define MIN(L, R)           ((L) < (R) ? (L) : (R))
#define MAX(L, R)           ((L) > (R) ? (L) : (R))
#define LSB(X)              (1ULL << (X))
#define SWAP(L, R)          {(L) ^= (R); (R) ^= (L); (L) ^= (R);}
#define SWAP_BITS(X, A, B)  {uint32_t XOR = (((X)>>(A)) ^ ((X)>>(B))) & 1; \
                                      (X) ^= (XOR<<(A)) | (XOR<<(B));}
#define LENGTH(ARR)         (sizeof(ARR) / sizeof(*ARR))

#ifdef __GNUC__
#  define UNUSED            __attribute__((unused))
#else
#  define UNUSED
#endif

#ifdef VXWM_DEBUG
#  define log(...) { \
  fprintf(VXWM_LOG_FILE, __VA_ARGS__); \
  fputc('\n', VXWM_LOG_FILE); \
}
void xassert(bool, const char *);
#else
#  define log(...)
#  define xassert(...)
#endif // VXWM_DEBUG

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
void xfree(void *);
void die(const char *);

#endif // VXWM_UTIL_H
