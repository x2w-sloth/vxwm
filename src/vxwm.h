#ifndef VXWM_H
#define VXWM_H

#define VXWM_VERSION_MAJOR    "0"
#define VXWM_VERSION_MINOR    "1"
#define VXWM_VERSION_PATCH    "3"
#define VXWM_VERSION          VXWM_VERSION_MAJOR "." \
                              VXWM_VERSION_MINOR "." \
                              VXWM_VERSION_PATCH

#define VXWM_CLN_MIN_W        30
#define VXWM_CLN_MIN_H        30
#define BORDER                2 * VXWM_CLN_BORDER_W
#define INPAGE(C)             (C->tag & (1 << fm->fp))

#include <xcb/xproto.h>

typedef enum { Prev = -1, This, Next, First, Last, Top, Bottom } pos_t;
typedef enum { PtrUngrabbed = 0, PtrMoveCln, PtrResizeCln } ptr_state_t;
typedef union { int i; uint32_t u32; pos_t p; const void *v; } arg_t;
typedef struct monitor monitor_t;
typedef struct page page_t;
typedef struct layout layout_t;
typedef struct layout_arg layout_arg_t;
typedef struct client client_t;
typedef struct keybind keybind_t;
typedef struct btnbind btnbind_t;
typedef void (*handler_t)(xcb_generic_event_t *);
typedef void (*bind_t)(const arg_t *);

#endif // VXWM_H
