#ifndef VXWM_H
#define VXWM_H

#define VXWM_VERSION_MAJOR    "0"
#define VXWM_VERSION_MINOR    "1"
#define VXWM_VERSION_PATCH    "1"
#define VXWM_VERSION          VXWM_VERSION_MAJOR "." \
                              VXWM_VERSION_MINOR "." \
                              VXWM_VERSION_PATCH

#define VXWM_PAGE_CAPACITY    16
#define VXWM_CLN_MIN_W        30
#define VXWM_CLN_MIN_H        30
#define VXWM_CLN_BORDER_W     4
#define VXWM_CLN_NORMAL_CLR   0x6B6B6B 
#define VXWM_CLN_FOCUS_CLR    0x00FFFF 
#define VXWM_CLN_INIT_WGHT    4 
#define VXWM_TAB_CAPACITY     16
#define VXWM_TAB_HEIGHT       8
#define VXWM_TAB_NORMAL_CLR   VXWM_CLN_NORMAL_CLR
#define VXWM_TAB_FOCUS_CLR    VXWM_CLN_FOCUS_CLR
#define VXWM_TAB_SELECT_CLR   0xFFA500
#define VXWM_LOG_FILE         stdout
#define VXWM_ERR_FILE         stderr

#define BORDER      2 * VXWM_CLN_BORDER_W
#define INPAGE(C)   (C->tag & (1 << fm->fp))

#include <xcb/xproto.h>

typedef enum { Prev = -1, This, Next, First, Last, Top, Bottom } target_t;
typedef enum { PtrUngrabbed = 0, PtrMoveCln, PtrResizeCln } ptr_state_t;
typedef union { int i; target_t t; const void *v; } arg_t;
typedef struct monitor monitor_t;
typedef struct page page_t;
typedef struct layout layout_t;
typedef struct client client_t;
typedef struct keybind keybind_t;
typedef struct btnbind btnbind_t;
typedef void (*handler_t)(xcb_generic_event_t *);
typedef void (*bind_t)(const arg_t *);

#endif // VXWM_H
