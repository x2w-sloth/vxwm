#ifndef VXWM_UTIL_H
#define VXWM_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>

#define MIN(L, R)       ((L) < (R) ? (L) : (R))
#define MAX(L, R)       ((L) > (R) ? (L) : (R))
#define LENGTH(ARR)     (sizeof(ARR) / sizeof(*ARR))
#define UNUSED(PTR)     (void)(PTR)
#define ATOM_NAME(ATOM) (uint16_t)strlen(ATOM), ATOM

#ifdef VXWM_DEBUG

# define log(...) { \
  fprintf(VXWM_LOG_FILE, __VA_ARGS__); \
  fputc('\n', VXWM_LOG_FILE); \
}

void xassert(bool, const char *);
void log_event(xcb_generic_event_t *);

#else

# define log(...)
# define log_event(...)
# define xassert(...)

#endif // VXWM_DEBUG

void *xmalloc(size_t);
void xfree(void *);
void die(const char *);

#endif // VXWM_UTIL_H
