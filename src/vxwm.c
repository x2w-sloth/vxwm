#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>
#include "vxwm.h"
#include "global.h"
#include "util.h"
#include "draw.h"
// a monitor corresponds to a physical display and contains pages
struct monitor {
  monitor_t *next;     // monitor linked list
  client_t *cln;       // clients under this monitor
  int np, fp;          // number of pages, index of focus page
};

// a page displays a subset of clients under a layout policy
struct page {
  const char *sym;     // page identifier string
  layout_t *lt;        // layout policy for this page
  int par[3];          // layout parameters
};

// a layout arranges tiled cilents in a page
struct layout {
  const char *sym;     // layout identifer string
  void (*fn)(const monitor_t *, int, int *); // arrangement function
};

// a client is one or more tabbed windows living under a monitor
struct client {
  client_t *next;      // client linked list
  xcb_window_t frame;  // client frame
  xcb_window_t *tab;   // window tabs
  uint32_t tag;        // page tag bitmask
  uint32_t sel;        // tab selection bitmask
  uint8_t tcap;        // maximum tab capacity
  int nt, ft;          // number of tabs, index of focus tab
  int x, y, w, h;      // client dimensions
  int px, py, pw, ph;  // previous client dimensions
  bool isfloat;        // client is floating
  bool isfullscr;      // client wishes to be fullscreen
};

// key bindings
struct keybind {
  uint16_t mod;
  xcb_keysym_t sym;
  bind_t fn;
  arg_t arg;
};

// mouse button bindings
struct btnbind {
  uint16_t mod;
  xcb_button_t btn;
  bind_t fn;
  arg_t arg;
};

enum {
  WmProtocols = 0,
  WmTakeFocus,
  WmDeleteWindow,
  WmAtomsLast,
};

enum {
  NetWmName = 0,
  NetWmWindowType,
  NetWmWindowTypeDialog,
  NetAtomsLast,
};

static void args(int, char **);
static void setup(void);
static void run(void);
static void cleanup(void);
static void atom_request(void);
static void atom_setup(void);
static xcb_keycode_t *keysym_to_keycodes(xcb_keysym_t);
static xcb_keysym_t keycode_to_keysym(xcb_keycode_t);
static void grab_keys(void);
static void ptr_motion(ptr_state_t);
static bool has_proto(xcb_window_t, xcb_atom_t);
static void send_msg(xcb_window_t, xcb_atom_t);
static xcb_atom_t get_atom_prop(xcb_window_t, xcb_atom_t);
static bool get_text_prop(xcb_window_t, xcb_atom_t, char *, size_t);
static void stack_win(xcb_window_t, target_t);
static void focus_win(xcb_window_t);
static void grant_configure_request(xcb_configure_request_event_t *);
// event handlers
static void on_key_press(xcb_generic_event_t *);
static void on_button_press(xcb_generic_event_t *);
static void on_motion_notify(xcb_generic_event_t *);
static void on_expose(xcb_generic_event_t *);
static void on_destroy_notify(xcb_generic_event_t *);
static void on_map_request(xcb_generic_event_t *);
static void on_configure_request(xcb_generic_event_t *);
// monitors and pages
static monitor_t *create_mon(void);
static void delete_mon(monitor_t *);
static void arrange_mon(monitor_t *);
// clients and tabs
static client_t *create_cln(void);
static void delete_cln(client_t *);
static void border_cln(client_t *, int);
static void frame_cln(client_t *);
static void attach_cln(client_t *);
static void detach_cln(client_t *);
static void focus_cln(client_t *);
static void raise_cln(client_t *);
static void fullscr_cln(client_t *, bool);
static void attach_tab(client_t *, xcb_window_t);
static void detach_tab(client_t *, int);
static void draw_tabs(client_t *);
static void kill_tab(xcb_window_t);
static void move_cln(client_t *, int, int);
static void resize_cln(client_t *, int, int);
static void move_resize_cln(client_t *, int, int, int, int);
static client_t *next_inpage(client_t *);
static client_t *prev_inpage(client_t *);
static client_t *next_tiled(client_t *);
static client_t *next_selected(client_t *);
static client_t *prev_cln(client_t *);
static client_t *win_to_cln(xcb_window_t);
// bindings
static void bn_quit(const arg_t *);
static void bn_spawn(const arg_t *);
static void bn_kill_tab(const arg_t *);
static void bn_swap_tab(const arg_t *);
static void bn_swap_cln(const arg_t *);
static void bn_move_cln(const arg_t *);
static void bn_resize_cln(const arg_t *);
static void bn_toggle_select(const arg_t *);
static void bn_toggle_float(const arg_t *);
static void bn_toggle_fullscr(const arg_t *);
static void bn_merge_cln(const arg_t *);
static void bn_split_cln(const arg_t *);
static void bn_focus_cln(const arg_t *);
static void bn_focus_tab(const arg_t *);
static void bn_focus_page(const arg_t *);
static void bn_set_param(const arg_t *);
static void bn_set_layout(const arg_t *);
// layouts
static void column(const monitor_t *, int, int *);
static void stack(const monitor_t *, int, int *);
// globals
static xcb_key_symbols_t *symbols;
static xcb_intern_atom_cookie_t wm_cookies[WmAtomsLast];
static xcb_intern_atom_cookie_t net_cookies[NetAtomsLast];
static xcb_atom_t wm_atom[WmAtomsLast];
static xcb_atom_t net_atom[NetAtomsLast];
static handler_t handler[XCB_NO_OPERATION];
static monitor_t *fm;
static client_t *fc;
static ptr_state_t ptr_state;
static bool ptr_first_motion;
static bool running;
static int ptr_x, ptr_y;
static int win_x, win_y, win_w, win_h;
static int ns;
static uint32_t vals[8], masks;
xcb_connection_t *conn;
xcb_screen_t *scr;
xcb_window_t root;

// will exist until lua configuration is implemented
#include "config.h"

void args(int argc, char **argv)
{
  if (argc > 1) {
    if (!strcmp(argv[1], "-v"))
      puts("version " VXWM_VERSION);
    else
      puts("options: -v");
    exit(EXIT_SUCCESS);
  }
}

void setup(void)
{
  xcb_void_cookie_t cookie;
  xcb_generic_error_t *error;

  // connect to X server
  conn = xcb_connect(NULL, NULL);
  if (!conn || xcb_connection_has_error(conn))
    die("failed to connect to x server");
  scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  if (!scr)
    die("failed to capture screen");

  // configure root window, check if another wm is running
  root = scr->root; log("root window: %d", root);
  masks = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
  cookie = xcb_change_window_attributes_checked(conn, root, XCB_CW_EVENT_MASK, &masks);
  error = xcb_request_check(conn, cookie);
  xcb_flush(conn);
  if (error)
    die("another window manager is running");

  // setup monitors and initialize drawing context
  fm = create_mon();
  draw_setup();

  // reqeust atoms, load symbols and grab keys on root window
  atom_request();
  symbols = xcb_key_symbols_alloc(conn);
  if (!symbols)
    die("failed to allocate key symbol table");
  atom_setup();
  grab_keys();
  xcb_flush(conn);

  // install event handlers
  handler[XCB_KEY_PRESS]       = on_key_press;
  handler[XCB_BUTTON_PRESS]    = on_button_press;
  handler[XCB_MOTION_NOTIFY]   = on_motion_notify;
//handler[XCB_ENTER_NOTIFY]    = on_enter_notify; // XCB_EVENT_MASK_ENTER_WINDOW
//handler[XCB_FOCUS_IN]        = on_focus_in;     // XCB_EVENT_MASK_FOCUS_CHANGE
  handler[XCB_EXPOSE]          = on_expose;
  handler[XCB_DESTROY_NOTIFY]  = on_destroy_notify;
//handler[XCB_UNMAP_NOTIFY]    = on_unmap_notify;
  handler[XCB_MAP_REQUEST]     = on_map_request;
  handler[XCB_CONFIGURE_REQUEST] = on_configure_request;
//handler[XCB_PROPERTY_NOTIFY] = on_property_notify;
//handler[XCB_CLIENT_MESSAGE]  = on_client_message;
//handler[XCB_MAPPING_NOTIFY]  = on_mapping_notify;

  // initialize status globals
  ptr_state = PtrUngrabbed;
  fc = NULL;
  ns = 0;
  running = true;
}

void run(void)
{
  xcb_generic_event_t *ge;
  uint8_t type;

  xcb_flush(conn);
  while (running && (ge = xcb_wait_for_event(conn))) {
    type = XCB_EVENT_RESPONSE_TYPE(ge);
    if (handler[type]) {
      log_event(ge);
      handler[type](ge);
    }
    xfree(ge);
  }
}

void cleanup(void)
{
  // TODO: kill remaining clients

  // free resources and disconnect
  delete_mon(fm);
  if (symbols)
    xcb_key_symbols_free(symbols);
  if (conn && !xcb_connection_has_error(conn))
    xcb_disconnect(conn);
}


void atom_request(void)
{
  wm_cookies[WmProtocols]    = xcb_intern_atom(conn, false, ATOM_NAME("WM_PROTOCOLS"));
  wm_cookies[WmTakeFocus]    = xcb_intern_atom(conn, false, ATOM_NAME("WM_TAKE_FOCUS"));
  wm_cookies[WmDeleteWindow] = xcb_intern_atom(conn, false, ATOM_NAME("WM_DELETE_WINDOW"));
  net_cookies[NetWmName]     = xcb_intern_atom(conn, false, ATOM_NAME("_NET_WM_NAME"));
  net_cookies[NetWmWindowType] = xcb_intern_atom(conn, false, ATOM_NAME("_NET_WM_WINDOW_TYPE"));
  net_cookies[NetWmWindowTypeDialog] = xcb_intern_atom(conn, false, ATOM_NAME("_NET_WM_WINDOW_TYPE_DIALOG"));
}

void atom_setup(void)
{
  xcb_intern_atom_reply_t *reply;
  int i;

  for (i = 0; i < WmAtomsLast; ++i)
    if ((reply = xcb_intern_atom_reply(conn, wm_cookies[i], NULL))) {
      wm_atom[i] = reply->atom;
      xfree(reply);
    } else {
      log("failed to retreive wm atom from server");
    }

  for (i = 0; i < NetAtomsLast; ++i)
    if ((reply = xcb_intern_atom_reply(conn, net_cookies[i], NULL))) {
      net_atom[i] = reply->atom;
      xfree(reply);
    } else {
      log("failed to retreive net atom from server");
    }
}

xcb_keycode_t *keysym_to_keycodes(xcb_keysym_t keysym)
{
  xcb_keycode_t *keycodes;

  keycodes = xcb_key_symbols_get_keycode(symbols, keysym); // this can be slow
  xassert(keycodes, "failed to allocate keycodes from symbol table");
  return keycodes; // remember to free this
}

xcb_keysym_t keycode_to_keysym(xcb_keycode_t keycode)
{
  return xcb_key_symbols_get_keysym(symbols, keycode, 0);
}

void grab_keys(void)
{
  xcb_keycode_t *keycodes;
  int i, j, n;

  xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_MOD_MASK_ANY);
  for (i = 0, n = LENGTH(keybinds); i < n; ++i) {
    keycodes = keysym_to_keycodes(keybinds[i].sym);
    for (j = 0; keycodes[j] != XCB_NO_SYMBOL; ++j)
      xcb_grab_key(conn, 1, root, keybinds[i].mod, keycodes[j],
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xfree(keycodes);
  }
}

void ptr_motion(ptr_state_t state)
{
  xassert(state == PtrMoveCln || state == PtrResizeCln, "bad pointer state");

  xcb_query_pointer_reply_t *ptr;
  xcb_get_geometry_reply_t *geo;

  ptr = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, root), NULL);
  xassert(ptr, "did not receive a reply from query pointer");
  ptr_x = ptr->root_x;
  ptr_y = ptr->root_y;
  xfree(ptr);

  geo = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, fc->frame), NULL);
  xassert(geo, "did not receive a reply from get geometery");
  win_x = geo->x;
  win_y = geo->y;
  win_w = geo->width;
  win_h = geo->height;
  xfree(geo);

  ptr_state = state; // see on_motion_notify
}

bool has_proto(xcb_window_t win, xcb_atom_t proto)
{
  xcb_icccm_get_wm_protocols_reply_t reply;
  xcb_get_property_cookie_t cookie;
  int i, n;

  cookie = xcb_icccm_get_wm_protocols_unchecked(conn, win, wm_atom[WmProtocols]);
  if (!xcb_icccm_get_wm_protocols_reply(conn, cookie, &reply, NULL)) {
    log("failed to retrieve wm protocols for %d", win);
    return false;
  }

  for (i = 0, n = reply.atoms_len; i < n && reply.atoms[i] != proto; ++i) ;
  xcb_icccm_get_wm_protocols_reply_wipe(&reply);
  return i != n;
}

void send_msg(xcb_window_t win, xcb_atom_t proto)
{
  xcb_client_message_event_t msg;

  msg.response_type = XCB_CLIENT_MESSAGE;
  msg.format = 32;
  msg.window = win;
  msg.type = wm_atom[WmProtocols];
  msg.data.data32[0] = proto;
  msg.data.data32[1] = XCB_TIME_CURRENT_TIME;
  xcb_send_event(conn, false, win, XCB_EVENT_MASK_NO_EVENT, (const char *)&msg);
}

xcb_atom_t get_atom_prop(xcb_window_t win, xcb_atom_t prop)
{
  xcb_get_property_reply_t *reply;
  xcb_get_property_cookie_t cookie;
  xcb_atom_t *atom;
  
  cookie = xcb_get_property(conn, 0, win, prop, XCB_ATOM_ATOM, 0, UINT32_MAX);
  reply = xcb_get_property_reply(conn, cookie, NULL);
  if (!reply || !(atom = xcb_get_property_value(reply)))
    return XCB_ATOM_NONE;
  xfree(reply);
  return *atom;
}

bool get_text_prop(xcb_window_t win, xcb_atom_t prop, char *text, size_t tsize)
{
  xcb_get_property_reply_t *reply;
  xcb_get_property_cookie_t cookie;
  size_t rsize;
  const char *buf;

  if (!text || tsize == 0)
    return false;
  cookie = xcb_get_property(conn, 0, win, prop, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT8_MAX);
  reply = xcb_get_property_reply(conn, cookie, NULL);
  if (!reply || (rsize = xcb_get_property_value_length(reply)) == 0 || rsize > tsize)
    return false;
  buf = xcb_get_property_value(reply);
  strncpy(text, buf, rsize);
  text[rsize] = '\0';
  xfree(reply);
  return true;
}

void stack_win(xcb_window_t win, target_t t)
{
  masks = XCB_CONFIG_WINDOW_STACK_MODE;
  vals[0] = t == Top ? XCB_STACK_MODE_ABOVE : XCB_STACK_MODE_BELOW;
  xcb_configure_window(conn, win, masks, vals);
}

void focus_win(xcb_window_t win)
{
  if (win != root && has_proto(win, wm_atom[WmTakeFocus]))
    send_msg(win, wm_atom[WmTakeFocus]);
  xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
}

void grant_configure_request(xcb_configure_request_event_t *e)
{
  int i = 0;
  masks = 0;
  // configure non-client window as requested
  if (e->value_mask & XCB_CONFIG_WINDOW_X) {
    masks |= XCB_CONFIG_WINDOW_X;
    vals[i++] = e->x;
  }
  if (e->value_mask & XCB_CONFIG_WINDOW_Y) {
    masks |= XCB_CONFIG_WINDOW_Y;
    vals[i++] = e->y;
  }
  if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
    masks |= XCB_CONFIG_WINDOW_WIDTH;
    vals[i++] = e->width;
  }
  if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
    masks |= XCB_CONFIG_WINDOW_HEIGHT;
    vals[i++] = e->height;
  }
  if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
    masks |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    vals[i++] = e->border_width;
  }
  if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
    masks |= XCB_CONFIG_WINDOW_SIBLING;
    vals[i++] = e->sibling;
  }
  if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
    masks |= XCB_CONFIG_WINDOW_STACK_MODE;
    vals[i++] = e->stack_mode;
  }
  xcb_configure_window(conn, e->window, masks, vals);
}

void on_key_press(xcb_generic_event_t *ge)
{
  xcb_key_press_event_t *e = (xcb_key_press_event_t *)ge;
  xcb_keysym_t keysym = keycode_to_keysym(e->detail);
  int i, n;

  for (i = 0, n = LENGTH(keybinds); i < n; ++i)
    if (keysym == keybinds[i].sym && e->state == keybinds[i].mod && keybinds[i].fn)
      keybinds[i].fn(&keybinds[i].arg);
}

void on_button_press(xcb_generic_event_t *ge)
{
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)ge;
  int i, n;

  focus_cln(win_to_cln(e->event));
  for (i = 0, n = LENGTH(btnbinds); i < n; ++i)
    if (e->detail == btnbinds[i].btn && e->state == btnbinds[i].mod && btnbinds[i].fn) {
      ptr_first_motion = true;
      btnbinds[i].fn(&btnbinds[i].arg);
    }
}

void on_motion_notify(xcb_generic_event_t *ge)
{
  xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)ge;
  bool move_or_resize = ptr_state == PtrMoveCln;
  int dx, dy, xw, yh;

  if (fc->isfullscr)
    return;

  if (ptr_first_motion) {
    ptr_first_motion = false;
    fc->isfloat = true;
    arrange_mon(fm);
  }

  dx = e->root_x - ptr_x;
  dy = e->root_y - ptr_y;
  xw = (move_or_resize ? win_x : win_w) + dx;
  yh = (move_or_resize ? win_y : win_h) + dy;
  (move_or_resize ? move_cln : resize_cln)(fc, xw, yh);
  xcb_flush(conn);
}

void on_expose(xcb_generic_event_t *ge)
{
  xcb_expose_event_t *e = (xcb_expose_event_t *)ge;
  client_t *c = win_to_cln(e->window);

  if (c)
    draw_tabs(c);
}

void on_destroy_notify(xcb_generic_event_t *ge)
{
  xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ge;
  client_t *c = win_to_cln(e->window);
  arg_t arg = { .t = This };
  int i;

  if (!c)
    return;

  for (i = 0; c->tab[i] != e->window; ++i) ;
  detach_tab(c, i);
  if (c->nt == 0) {
    if (fc == c) {
      fc = NULL; // so we don't call draw_tabs with 0 tabs, inside focus_cln
      focus_cln(c->next ? c->next : prev_cln(c));
    }
    detach_cln(c);
    delete_cln(c);
    arrange_mon(fm);
  } else
    bn_focus_tab(&arg);
}

void on_map_request(xcb_generic_event_t *ge)
{
  xcb_map_request_event_t *e = (xcb_map_request_event_t *)ge;
  xcb_get_window_attributes_cookie_t cookie;
  xcb_get_window_attributes_reply_t *wa;
  xcb_atom_t win_type;
  client_t *c = win_to_cln(e->window);

  cookie = xcb_get_window_attributes(conn, e->window);
  wa = xcb_get_window_attributes_reply(conn, cookie, NULL);

  if (!wa || wa->override_redirect) {
    xfree(wa);
    return;
  }
  if (!c) {
    c = create_cln();
    attach_tab(c, e->window);
    attach_cln(c);
    win_type = get_atom_prop(c->tab[c->ft], net_atom[NetWmWindowType]);
    if (win_type == net_atom[NetWmWindowTypeDialog])
      c->isfloat = true;
    arrange_mon(fm);
    xcb_map_window(conn, e->window);
    focus_cln(c);
    xcb_flush(conn);
  }
}

void on_configure_request(xcb_generic_event_t *ge)
{
  xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ge;
  client_t *c;
  int x, y, w, h;

  if ((c = win_to_cln(e->window)) && c->isfloat) {
    xassert(e->window == c->tab[c->ft], "configured window is not focus tab");
    x = c->x;
    y = c->y;
    w = c->w;
    h = c->h;
    if (e->value_mask & XCB_CONFIG_WINDOW_X)
      x = e->x;
    if (e->value_mask & XCB_CONFIG_WINDOW_Y)
      y = e->y;
    if (e->value_mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y))
      move_cln(c, x, y);
    if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
      w = e->width;
    if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
      h = e->height;
    if (e->value_mask & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))
      resize_cln(c, w, h);
  } else if (!c)
    grant_configure_request(e);
  xcb_flush(conn);
}

monitor_t *create_mon(void)
{
  monitor_t *m;

  m = xmalloc(sizeof(monitor_t));
  m->next = NULL;
  m->cln = NULL;
  m->np = LENGTH(pages);
  m->fp = 0;
  return m;
}

void delete_mon(monitor_t *m)
{
  // xassert(!m->cln, "deleting a monitor with clients");
  xfree(m);
}

void arrange_mon(monitor_t *m)
{
  xassert(m, "bad call to arrange_mon");
  client_t *c;
  int n;

  for (c = next_inpage(m->cln), n = 0; c; c = next_inpage(c->next)) {
    // halts at the first client in page that wishes to be fullscreen
    if (c->isfullscr) {
      fullscr_cln(c, true);
      xcb_flush(conn);
      return;
    }
    // restack clients, count tiled clients
    if (!c->isfloat) {
      stack_win(c->frame, Bottom);
      ++n;
    } else
      stack_win(c->frame, Top);
  }

  // the layout may modify page parameters as they see fit
  if (n != 0)
    pages[m->fp].lt->fn(m, n, pages[m->fp].par);
  xcb_flush(conn);
  log("arranged page %s with %s", pages[m->fp].sym, pages[m->fp].lt->sym);
}


client_t *create_cln()
{
  xassert(fm, "no focus monitor");
  client_t *c;

  c = xmalloc(sizeof(client_t));
  c->next = NULL;
  c->tab = xmalloc(sizeof(xcb_window_t));
  c->tag = 1 << fm->fp;
  c->sel = 0;
  c->tcap = 1;
  c->nt = 0;
  c->ft = 0;
  c->isfloat = false;
  c->isfullscr = false;
  c->x = c->px = 0;
  c->y = c->py = 0;
  c->w = c->pw = VXWM_CLN_MIN_W;
  c->h = c->ph = VXWM_CLN_MIN_H;

  frame_cln(c);
  xcb_map_window(conn, c->frame);
  log("created client frame: %d", c->frame);
  return c;
}

void delete_cln(client_t *c)
{
  xassert(c->nt == 0, "deleting a client with tabs");

  xcb_unmap_window(conn, c->frame);
  xcb_destroy_window(conn, c->frame);
  xcb_flush(conn);
  log("destroy frame: %d", c->frame);
  xfree(c->tab);
  xfree(c);
}

void border_cln(client_t *c, int width)
{
  xassert(c && width >= 0, "bad call to border_cln");

  masks = XCB_CONFIG_WINDOW_BORDER_WIDTH;
  xcb_configure_window(conn, c->frame, masks, &width);
}

void frame_cln(client_t *c)
{
  uint32_t bw = 3, bp = VXWM_CLN_NORMAL_CLR;
  int i, n;

  c->frame = xcb_generate_id(conn);
  xcb_create_window(conn, scr->root_depth,
                    c->frame, root, 0, 0, 1, 1, bw,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    scr->root_visual, XCB_CW_BORDER_PIXEL, &bp);

  masks = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
          XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
          XCB_EVENT_MASK_EXPOSURE;
  xcb_change_window_attributes(conn, c->frame, XCB_CW_EVENT_MASK, &masks);

  border_cln(c, VXWM_CLN_BORDER_W);

  masks = XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_BUTTON_RELEASE |
          XCB_EVENT_MASK_BUTTON_MOTION;
  for (i = 0, n = LENGTH(btnbinds); i < n; ++i)
    xcb_grab_button(conn, 0, c->frame, masks, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                    XCB_NONE, XCB_NONE, btnbinds[i].btn, btnbinds[i].mod);
}

void attach_cln(client_t *c)
{
  c->next = fm->cln;
  fm->cln = c;
}

void detach_cln(client_t *c)
{
  client_t **cc;
  for (cc = &fm->cln; *cc && *cc != c; cc = &(*cc)->next) ;
  *cc = c->next;
}

void focus_cln(client_t *c)
{
  uint32_t fclr = VXWM_CLN_FOCUS_CLR;
  uint32_t nclr = VXWM_CLN_NORMAL_CLR;
  client_t *pf = NULL;

  if (fc == c)
    return;

  // update old focus client
  if (fc) {
    pf = fc;
    fc = c;
    xcb_change_window_attributes(conn, pf->frame, XCB_CW_BORDER_PIXEL, &nclr);
    draw_tabs(pf);
  }

  // assign new focus client or loose focus
  if ((fc = c)) {
    raise_cln(fc); // TODO: raise_on_focus option
    xcb_change_window_attributes(conn, fc->frame, XCB_CW_BORDER_PIXEL, &fclr);
    focus_win(fc->tab[fc->ft]);
    draw_tabs(fc);
    log("new focus: %d", fc->frame);
  } else {
    focus_win(root);
    log("loosing focus");
  }
}

void raise_cln(client_t *c)
{
  xassert(c, "bad call to raise_cln");
  stack_win(c->frame, Top);
  draw_tabs(c);
}

void fullscr_cln(client_t *c, bool fullscr)
{
  xassert(c, "bad call to fullscr_cln");

  if (fullscr) {
    stack_win(c->frame, Top);
    border_cln(c, 0);
    move_resize_cln(c, 0, 0, scr->width_in_pixels, scr->height_in_pixels);
  } else {
    border_cln(c, VXWM_CLN_BORDER_W);
    move_resize_cln(c, c->px, c->py, c->pw, c->ph);
  }
}

void attach_tab(client_t *c, xcb_window_t win)
{
  if (c->nt == c->tcap) {
    c->tcap <<= 1;
    c->tab = xrealloc(c->tab, sizeof(xcb_window_t) * c->tcap);
  }
  c->tab[c->nt++] = win;
  xcb_reparent_window(conn, win, c->frame, 0, VXWM_TAB_HEIGHT);
}

void detach_tab(client_t *c, int i)
{
  xassert(c && i < c->nt, "bad call to detach_tab");
  uint32_t lsb;

  if (c->sel & (1 << i))
    --ns;
  
  // preserve selection mask
  lsb = (1 << i) - 1;
  c->sel = ((c->sel >> 1) & ~lsb) + (c->sel & lsb);

  for (; i < c->nt - 1; ++i)
    c->tab[i] = c->tab[i + 1];
  c->nt--;
  c->ft = MAX(c->ft - 1, 0);
}

void draw_tabs(client_t *c)
{
  xassert(c && c->nt > 0, "bad call to draw_tabs");
  int tw = c->w / c->nt, sw = VXWM_TAB_HEIGHT / 2;
  int i;

  draw_rect(0, 0, c->w, VXWM_TAB_HEIGHT, VXWM_TAB_NORMAL_CLR);
  if (c == fc)
    draw_rect(tw * c->ft, 0, tw, VXWM_TAB_HEIGHT, VXWM_TAB_FOCUS_CLR);
  for (i = 0; i < c->nt; ++i)
    if (c->sel & (1 << i))
      draw_rect((i + 0.25) * tw, sw / 2, tw / 2, sw, VXWM_TAB_SELECT_CLR);
  draw_copy(c->frame, 0, 0, c->w, VXWM_TAB_HEIGHT);
}

void kill_tab(xcb_window_t win)
{
  if (has_proto(win, wm_atom[WmDeleteWindow]))
    send_msg(win, wm_atom[WmDeleteWindow]);
  else
    xcb_kill_client(conn, win);
}

void move_cln(client_t *c, int x, int y)
{
  xassert(c, "bad call to move_cln");

  c->px = c->x;
  c->py = c->y;
  masks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
  vals[0] = c->x = x;
  vals[1] = c->y = y;
  xcb_configure_window(conn, c->frame, masks, vals);
}

void resize_cln(client_t *c, int w, int h)
{
  xassert(c, "bad call to resize_cln");

  if (w < VXWM_CLN_MIN_W || h < VXWM_CLN_MIN_H)
    return;

  c->pw = c->w;
  c->ph = c->h;
  masks = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  vals[0] = c->w = w;
  vals[1] = c->h = h;
  xcb_configure_window(conn, c->frame, masks, vals);
  xcb_configure_window(conn, c->tab[c->ft], masks, vals);
  draw_tabs(c);
}

void move_resize_cln(client_t *c, int x, int y, int w, int h)
{
  move_cln(c, x, y);
  resize_cln(c, w, h);
}

client_t *next_inpage(client_t *c)
{
  for (; c && !INPAGE(c); c = c->next) ;
  return c;
}

client_t *prev_inpage(client_t *c)
{
  client_t *p;
  for (p = next_inpage(fm->cln); p && (!INPAGE(c) || p->next != c); p = p->next) ;
  return p;
}

client_t *next_tiled(client_t *c)
{
  for (; c && (!INPAGE(c) || c->isfloat); c = c->next) ;
  return c;
}

client_t *next_selected(client_t *c)
{
  for (; c && (!INPAGE(c) || c->sel == 0); c = c->next) ;
  return c;
}

client_t *prev_cln(client_t *c)
{
  client_t *p;
  for (p = fm->cln; p && p->next != c; p = p->next) ;
  return p;
}

client_t *win_to_cln(xcb_window_t win)
{
  client_t *c;
  int i;

  for (c = fm->cln; c; c = c->next) {
    if (c->frame == win)
      return c;
    for (i = 0; i < c->nt; ++i)
      if (c->tab[i] == win)
        return c;
  }
  return NULL;
}

void bn_quit(const arg_t *arg)
{
  UNUSED(arg);
  running = false;
}

void bn_spawn(const arg_t *arg)
{
  char **args = (char **)arg->v;
  pid_t pid = fork();

  if (pid == -1)
    die("fork failed");
  else if (pid == 0) {
    execvp(*args, args);
    _exit(EXIT_SUCCESS);
  }
}

void bn_kill_tab(const arg_t *arg)
{
  UNUSED(arg);
  client_t *c;
  int i;

  if (!fc)
    return;

  if (ns > 0) { // consume selection
    for (c = next_selected(fm->cln); c; c = next_selected(c->next))
      for (i = 0; i < c->nt; ++i) if (c->sel & (1 << i))
        kill_tab(c->tab[i]);
  } else
    kill_tab(fc->tab[fc->ft]);
  xcb_flush(conn);
}

void bn_swap_tab(const arg_t *arg)
{
  int swp = -1;

  if (!fc || fc->nt == 1)
    return;

  switch (arg->t) {
    case First:
    case Top:
      swp = 0;
      break;
    case Last:
    case Bottom:
      swp = fc->nt - 1;
      break;
    case Prev:
      swp = fc->ft == 0 ? fc->nt - 1 : fc->ft - 1;
      break;
    case Next:
      swp = fc->ft == fc->nt - 1 ? 0 : fc->ft + 1;
      break;
    default:
      xassert(false, "bad argument in bn_swap_tab");
      return;
  }
  SWAP_BITS(fc->sel, swp, fc->ft);
  SWAP(fc->tab[swp], fc->tab[fc->ft])
  bn_focus_tab(arg);
}

void bn_swap_cln(const arg_t *arg)
{
  client_t *p = NULL, *c = next_tiled(fm->cln);

  if (!fc || fc->isfloat || !next_tiled(c->next))
    return;

  switch (arg->t) {
    case First:
      if (next_tiled(fm->cln) == fc)
        return;
      c = fm->cln->next;
      fm->cln->next = fc->next;
      fc->next = fm->cln;
      fm->cln = c;
      break;
    case Top: // not necessarily a 'swap'
      break;
    case Last:
    case Bottom:
      if ((p = next_tiled(fc->next)) == NULL)
        return;
      while (next_tiled(p->next))
        p = next_tiled(p->next);
      break;
    case Prev:
      if ((c = next_tiled(fm->cln)) == fc) // wrap to last tiled
        for (p = c, c = NULL; next_tiled(p->next); p = next_tiled(p->next)) ;
      while (c && next_tiled(c->next) != fc) {
        p = c;
        c = next_tiled(c->next);
      }
      break;
    case Next:
      p = next_tiled(fc->next);
      break;
    default:
      xassert(false, "bad argument in bn_swap_cln");
      return;
  }
  // reattach focus client
  detach_cln(fc);
  if (p) {
    fc->next = p->next;
    p->next = fc;
  } else {
    fc->next = fm->cln;
    fm->cln = fc;
  }
  arrange_mon(fm);
}

void bn_move_cln(const arg_t *arg)
{
  UNUSED(arg);

  ptr_motion(PtrMoveCln);
}

void bn_resize_cln(const arg_t *arg)
{
  UNUSED(arg);

  ptr_motion(PtrResizeCln);
}

void bn_toggle_select(const arg_t *arg)
{
  UNUSED(arg);

  if (!fc)
    return;

  fc->sel ^= (1 << fc->ft);
  ns += (fc->sel & (1 << fc->ft)) ? +1 : -1;
  draw_tabs(fc);
  xcb_flush(conn);
}

void bn_toggle_float(const arg_t *arg)
{
  UNUSED(arg);

  if (!fc)
    return;

  fc->isfloat = !fc->isfloat;
  arrange_mon(fm);
}

void bn_toggle_fullscr(const arg_t *arg)
{
  UNUSED(arg);

  if (!fc)
    return;

  if (fc->isfullscr) {
    fullscr_cln(fc, false);
    fc->isfullscr = false;
  } else
    fc->isfullscr = true;
  arrange_mon(fm);
}

void bn_merge_cln(const arg_t *arg)
{
  UNUSED(arg);
  client_t *c, *d, *mc;
  xcb_window_t win;
  int i;

  if (!fc || ns == 0)
    return;

  // determine merge destination
  switch (arg->t) {
    case This:
      mc = fc;
      break;
    default:
      mc = create_cln();
      attach_cln(mc);
      break;
  }
 
  c = next_selected(fm->cln);
  while (c) {
    while (c->sel) {
      for (i = 0; !(c->sel & (1 << i)); ++i) ;
      win = c->tab[i];
      detach_tab(c, i);
      attach_tab(mc, win);
    }
    if (c->nt == 0) {
      d = c;
      c = next_selected(c->next);
      detach_cln(d);  
      delete_cln(d);
    } else {
      draw_tabs(c);
      c = next_selected(c->next);
    }
  }
  xassert(ns == 0, "bad selection counting");
  stack_win(mc->tab[mc->ft], Top);
  focus_cln(mc);
  arrange_mon(fm);
}

void bn_split_cln(const arg_t *arg)
{
  UNUSED(arg);
  client_t *c, *sc = NULL;
  xcb_window_t win;
  int i;

  if (!fc || (ns == 0 && fc->nt == 1))
    return;

  if (ns == 0) { // split focus tab from focus client
    sc = create_cln();
    attach_cln(sc);
    win = fc->tab[fc->ft];
    detach_tab(fc, fc->ft);
    attach_tab(sc, win);
  } else for (c = next_selected(fm->cln); c; c = next_selected(c->next))
    while (c->sel) { // consume selection
      if (c->nt == 1) {
        --ns;
        c->sel = 0;
        break;
      }
      sc = create_cln();
      attach_cln(sc);
      for (i = 0; !(c->sel & (1 << i)); ++i) ;
      win = c->tab[i];
      detach_tab(c, i);
      attach_tab(sc, win);
    }
  xassert(ns == 0, "bad selection counting");
  focus_cln(sc ? sc : fc);
  arrange_mon(fm);
}

void bn_focus_cln(const arg_t *arg)
{
  client_t *c = NULL;

  // lock focus when fullscreen
  if (!fc || fc->isfullscr)
    return;

  switch (arg->t) {
    case Prev:
      if ((c = prev_inpage(fc)) == NULL) // wrap to last in page
        for (c = fc; next_inpage(c->next); c = next_inpage(c->next)) ;
      break;
    case This:
      c = fc;
      break;
    case Next:
      if ((c = next_inpage(fc->next)) == NULL) // wrap to first in page
        c = next_inpage(fm->cln);
      break;
    case First:
    case Top:
      c = next_inpage(fm->cln);
      break;
    case Last:
    case Bottom:
      for (c = fc; next_inpage(c->next); c = next_inpage(c->next)) ;
      break;
  }
  focus_cln(c);
  xcb_flush(conn);
}

void bn_focus_tab(const arg_t *arg)
{
  target_t t = arg->t;
  xcb_window_t win;
  int n;

  if (!fc)
    return;

  n = fc->nt;
  switch (t) {
    case First:
    case Top:
      fc->ft = 0;
      break;
    case Last:
    case Bottom:
      fc->ft = n - 1;
      break;
    case Prev:
    case This:
    case Next:
      fc->ft = (fc->ft + (int)t + n) % n;
      break;
  }
  win = fc->tab[fc->ft];

  // resize tab window to match client dimensions
  masks = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  vals[0] = fc->w;
  vals[1] = fc->h;
  xcb_configure_window(conn, win, masks, vals);

  // raise tab window and redraw client tabs
  stack_win(win, Top);
  focus_win(win);
  draw_tabs(fc);

  xcb_flush(conn);
  log("focusing tab %d (%d/%d)", win, fc->ft + 1, fc->nt);
}

void bn_focus_page(const arg_t *arg)
{
  client_t *c;

  if (fm->fp == arg->i)
    return;
  
  // lose selection when switching pages
  for (c = next_inpage(fm->cln); c; c = next_inpage(c->next)) {
    c->sel = 0;
    move_cln(c, scr->width_in_pixels, 0);
  }
  ns = 0;

  log("focus page %s", pages[arg->i].sym);
  fm->fp = arg->i;
  arrange_mon(fm);
}

void bn_set_param(const arg_t *arg)
{
  int *v = (int *)arg->v;
  
  pages[fm->fp].par[v[0]] += v[1];
  arrange_mon(fm);
}

void bn_set_layout(const arg_t *arg)
{
  xassert(arg->i < (int)LENGTH(layouts), "bad call to bn_set_layout");

  pages[fm->fp].lt = &layouts[arg->i];
  arrange_mon(fm);
}

// column layout
//   arrange clients in columns
//   parameter 0: number of columns
//   excess clients are piled in the right-most column
void column(const monitor_t *m, int n, int *par)
{
  client_t *c;
  int scrw = scr->width_in_pixels, scrh = scr->height_in_pixels;
  int i, h, cols, colw;

  if (par[0] <= 0)
    par[0] = 1;

  cols = MIN(par[0], n);
  colw = scrw / cols;
  h = scrh / (n - cols + 1);

  for (c = next_tiled(m->cln), i = 0; c; c = next_tiled(c->next), ++i)
    if (i < cols - 1)
      move_resize_cln(c, i * colw, 0, colw - BORDER, scrh - BORDER);
    else
      move_resize_cln(c, scrw - colw, h * (i - cols + 1), colw - BORDER, h - BORDER);
}

// stack layout
//   arrange clients in two stacks
//   parameter 0: number of clients in left stack
//   excess clients are piled in right stack 
void stack(const monitor_t *m, int n, int *par)
{
  client_t *c;
  int scrw = scr->width_in_pixels, scrh = scr->height_in_pixels;
  int i, ln, lw, rn, rw;

  if (par[0] <= 0)
    par[0] = 1;

  ln = MIN(par[0], n);
  rn = n - ln;
  if (n <= ln) {
    lw = scrw;
    rw = 0;
  } else
    lw = rw = scrw / 2;

  for (c = next_tiled(m->cln), i = 0; c; c = next_tiled(c->next), ++i)
    if (i < ln)
      move_resize_cln(c, 0, scrh / ln * i, lw - BORDER, scrh / ln - BORDER);
    else
      move_resize_cln(c, lw, scrh / rn * (i - ln), rw - BORDER, scrh / rn - BORDER);
}

int main(int argc, char *argv[])
{
  args(argc, argv);
  setup();
  run();
  cleanup();
  return EXIT_SUCCESS;
}

// vim: ts=2:sw=2:et
