#ifndef VXWM_UTIL_H
#define VXWM_UTIL_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>

#define MIN(L, R)           ((L) < (R) ? (L) : (R))
#define MAX(L, R)           ((L) > (R) ? (L) : (R))
#define LSB(X)              (1ULL << (X))
#define SWAP(L, R)          {(L) ^= (R); (R) ^= (L); (L) ^= (R);}
#define SWAP_BITS(X, A, B)  {uint32_t XOR = (((X)>>(A)) ^ ((X)>>(B))) & 1; \
                                      (X) ^= (XOR<<(A)) | (XOR<<(B));}
#define LENGTH(ARR)         (sizeof(ARR) / sizeof(*ARR))

#ifdef __GNUC__
#  define UNUSED            __attribute__((unused))
#  define INLINE            __attribute__((always_inline))
#else
#  define UNUSED
#  define INLINE            inline
#endif

#ifdef VXWM_DEBUG
#  define VXWM_LOG_FILE            stdout
#  define VXWM_LOG_LEVEL_WARN      0
#  define VXWM_LOG_LEVEL_INFO      1
#  define VXWM_LOG_LEVEL_VERBOSE   2
#  ifndef VXWM_LOG_LEVEL
#    define VXWM_LOG_LEVEL         VXWM_LOG_LEVEL_VERBOSE
#  endif
#  define LOG(...)  fprintf(VXWM_LOG_FILE, __VA_ARGS__);
#  define LOGW(...) if (VXWM_LOG_LEVEL >= VXWM_LOG_LEVEL_WARN) LOG(__VA_ARGS__);
#  define LOGI(...) if (VXWM_LOG_LEVEL >= VXWM_LOG_LEVEL_INFO) LOG(__VA_ARGS__);
#  define LOGV(...) if (VXWM_LOG_LEVEL >= VXWM_LOG_LEVEL_VERBOSE) LOG(__VA_ARGS__);
void xassert(bool, const char *);
#else
#  define LOGW(...) ;
#  define LOGI(...) ;
#  define LOGV(...) ;
#  define xassert(...)
#endif // VXWM_DEBUG

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
void xfree(void *);
void die(const char *);

#endif // VXWM_UTIL_H
