#ifndef VXWM_H
#define VXWM_H

// CORE HEADER
//   version declaration
//   expose the global session struct
//   typedef of core enums and structs

#define VXWM_VERSION_MAJOR    "0"
#define VXWM_VERSION_MINOR    "2"
#define VXWM_VERSION_PATCH    "0"
#define VXWM_VERSION          VXWM_VERSION_MAJOR "." \
                              VXWM_VERSION_MINOR "." \
                              VXWM_VERSION_PATCH "-alpha"

#include <xcb/xproto.h>

typedef enum {
  Prev = -1,
  This,
  Next,
  First,
  Last,
  Top,
  Bottom
} pos_t;

typedef enum {
  PtrUngrabbed = 0,
  PtrMoveCln,
  PtrResizeCln
} ptr_state_t;

enum {
  WmProtocols = 0,
  WmTakeFocus,
  WmDeleteWindow,
  WmState,
  WmAtomsLast,
};

enum {
  NetWmName = 0,
  NetWmWindowType,
  NetWmWindowTypeDialog,
  NetAtomsLast,
};

typedef struct session {
  xcb_connection_t *conn;
  xcb_screen_t *scr;
  xcb_window_t root;
  xcb_atom_t wm_atom[WmAtomsLast];
  xcb_atom_t net_atom[NetAtomsLast];
} session_t;

extern session_t sn;

typedef struct monitor monitor_t;
typedef struct page page_t;
typedef struct layout_arg layout_arg_t;
typedef struct client client_t;
typedef struct keybind keybind_t;
typedef struct btnbind btnbind_t;
typedef void (*layout_t)(const layout_arg_t *);
typedef void (*handler_t)(xcb_generic_event_t *);
typedef union { int i; uint32_t u32; pos_t p; layout_t lt; const void *v; } arg_t;
typedef void (*bind_t)(const arg_t *);

#endif // VXWM_H
