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

#define VXWM_ROOT_EVENT_MASK    (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |\
                                 XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define VXWM_FRAME_EVENT_MASK   (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |\
                                 XCB_EVENT_MASK_EXPOSURE)

#define VXWM_WIN_EVENT_MASK     (XCB_EVENT_MASK_STRUCTURE_NOTIFY)

// a monitor corresponds to a physical display and contains pages
struct monitor {
  monitor_t *next;     // monitor linked list
  client_t *cln;       // clients under this monitor
  int np, fp;          // number of pages, index of focus page
  int lx, ly, lw, lh;  // layout space where tiled clients are arranged in
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
  void (*fn)(const layout_arg_t *); // arrangement function
};

// context for layout arrangement function
struct layout_arg {
  monitor_t *mon;      // the monitor we are arranging
  int *par;            // parameters from the focus page
  int ntiled;          // number of tiled clients in focus page
};

// a client is one or more tabbed windows living under a monitor
struct client {
  client_t *next;      // client linked list
  xcb_window_t frame;  // client frame
  xcb_window_t *tab;   // window tabs
  uint32_t tag;        // page tag bitmask
  uint64_t sel;        // tab selection bitmask
  int tcap;            // maximum tab capacity
  int nt, ft;          // number of tabs, index of focus tab
  int x, y, w, h;      // client dimensions
  int px, py, pw, ph;  // previous client dimensions
  bool isfloating;     // client is floating
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
static void atom_setup(void);
static xcb_keycode_t *keysym_to_keycodes(xcb_keysym_t);
static xcb_keysym_t keycode_to_keysym(xcb_keycode_t);
static void grab_keys(void);
static void ptr_motion(ptr_state_t);
static bool has_proto(xcb_window_t, xcb_atom_t);
static void send_msg(xcb_window_t, xcb_atom_t);
static xcb_atom_t get_atom_prop(xcb_window_t, xcb_atom_t);
static bool get_text_prop(xcb_window_t, xcb_atom_t, char *, size_t);
static void win_stack(xcb_window_t, target_t);
static void win_focus(xcb_window_t);
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
static monitor_t *mon_create(void);
static void mon_delete(monitor_t *);
static void mon_arrange(monitor_t *);
// clients and tabs
static client_t *cln_create(void);
static void cln_manage(xcb_window_t);
static void cln_delete(client_t *);
static void cln_unmanage(client_t *);
static void cln_set_border(client_t *, int);
static void cln_add_frame(client_t *);
static void cln_attach(client_t *);
static void cln_detach(client_t *);
static void cln_set_focus(client_t *);
static void cln_raise(client_t *);
static void cln_set_fullscr(client_t *, bool);
static void cln_move(client_t *, int, int);
static void cln_resize(client_t *, int, int);
static void cln_move_resize(client_t *, int, int, int, int);
static void cln_show_hide(monitor_t *);
static void tab_attach(client_t *, xcb_window_t);
static void tab_detach(client_t *, int);
static void tab_draw(client_t *);
static void tab_kill(xcb_window_t);
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
static void bn_set_tag(const arg_t *);
static void bn_set_param(const arg_t *);
static void bn_set_layout(const arg_t *);
// layouts
static void column(const layout_arg_t *);
static void stack(const layout_arg_t *);
// globals
static xcb_key_symbols_t *symbols;
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
  root = scr->root;
  vals[0] = VXWM_ROOT_EVENT_MASK;
  cookie = xcb_change_window_attributes_checked(conn, root, XCB_CW_EVENT_MASK, vals);
  error = xcb_request_check(conn, cookie);
  xcb_flush(conn);
  if (error)
    die("another window manager is running");

  // setup monitors, initialize drawing context and atoms
  fm = mon_create();
  draw_setup();
  atom_setup();

  // load symbols and grab keys on root window
  symbols = xcb_key_symbols_alloc(conn);
  if (!symbols)
    die("failed to allocate key symbol table");
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
    if (handler[type])
      handler[type](ge);
    xfree(ge);
  }
}

void cleanup(void)
{
  // TODO: kill remaining clients

  // free resources and disconnect
  draw_cleanup();
  mon_delete(fm);
  if (symbols)
    xcb_key_symbols_free(symbols);
  if (conn && !xcb_connection_has_error(conn))
    xcb_disconnect(conn);
}


#define ATOM(NAME) xcb_intern_atom(conn, false, (uint16_t)strlen(NAME), NAME)
void atom_setup(void)
{
  xcb_intern_atom_cookie_t wm_cookies[WmAtomsLast];
  xcb_intern_atom_cookie_t net_cookies[NetAtomsLast];
  xcb_intern_atom_reply_t *reply;
  int i;

  // request atoms
  wm_cookies[WmProtocols]    = ATOM("WM_PROTOCOLS");
  wm_cookies[WmTakeFocus]    = ATOM("WM_TAKE_FOCUS");
  wm_cookies[WmDeleteWindow] = ATOM("WM_DELETE_WINDOW");
  net_cookies[NetWmName]     = ATOM("_NET_WM_NAME");
  net_cookies[NetWmWindowType] = ATOM("_NET_WM_WINDOW_TYPE");
  net_cookies[NetWmWindowTypeDialog] = ATOM("_NET_WM_WINDOW_TYPE_DIALOG");

  for (i = 0; i < WmAtomsLast; i++)
    if ((reply = xcb_intern_atom_reply(conn, wm_cookies[i], NULL))) {
      wm_atom[i] = reply->atom;
      xfree(reply);
    }

  for (i = 0; i < NetAtomsLast; i++)
    if ((reply = xcb_intern_atom_reply(conn, net_cookies[i], NULL))) {
      net_atom[i] = reply->atom;
      xfree(reply);
    }
}
#undef ATOM

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
  for (i = 0, n = LENGTH(keybinds); i < n; i++) {
    keycodes = keysym_to_keycodes(keybinds[i].sym);
    for (j = 0; keycodes[j] != XCB_NO_SYMBOL; j++)
      xcb_grab_key(conn, 1, root, keybinds[i].mod, keycodes[j],
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xfree(keycodes);
  }
}

void ptr_motion(ptr_state_t state)
{
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

  for (i = 0, n = reply.atoms_len; i < n && reply.atoms[i] != proto; i++) ;
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

void win_stack(xcb_window_t win, target_t t)
{
  masks = XCB_CONFIG_WINDOW_STACK_MODE;
  vals[0] = t == Top ? XCB_STACK_MODE_ABOVE : XCB_STACK_MODE_BELOW;
  xcb_configure_window(conn, win, masks, vals);
}

void win_focus(xcb_window_t win)
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

  for (i = 0, n = LENGTH(keybinds); i < n; i++)
    if (keysym == keybinds[i].sym && e->state == keybinds[i].mod && keybinds[i].fn)
      keybinds[i].fn(&keybinds[i].arg);
}

void on_button_press(xcb_generic_event_t *ge)
{
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)ge;
  int i, n;

  cln_set_focus(win_to_cln(e->event));
  for (i = 0, n = LENGTH(btnbinds); i < n; i++)
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
    fc->isfloating = true;
    mon_arrange(fm);
  }

  dx = e->root_x - ptr_x;
  dy = e->root_y - ptr_y;
  xw = (move_or_resize ? win_x : win_w) + dx;
  yh = (move_or_resize ? win_y : win_h) + dy;
  (move_or_resize ? cln_move : cln_resize)(fc, xw, yh);
  xcb_flush(conn);
}

void on_expose(xcb_generic_event_t *ge)
{
  xcb_expose_event_t *e = (xcb_expose_event_t *)ge;
  client_t *c = win_to_cln(e->window);

  if (c)
    tab_draw(c);
}

void on_destroy_notify(xcb_generic_event_t *ge)
{
  xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ge;
  client_t *c = win_to_cln(e->window);
  arg_t arg = { .t = This };
  int i;

  if (!c)
    return;

  for (i = 0; c->tab[i] != e->window; i++) ;
  tab_detach(c, i);
  if (c->nt == 0)
    cln_unmanage(c);
  else
    bn_focus_tab(&arg);
}

void on_map_request(xcb_generic_event_t *ge)
{
  xcb_map_request_event_t *e = (xcb_map_request_event_t *)ge;
  xcb_get_window_attributes_cookie_t cookie;
  xcb_get_window_attributes_reply_t *wa;
  client_t *c = win_to_cln(e->window);

  cookie = xcb_get_window_attributes(conn, e->window);
  wa = xcb_get_window_attributes_reply(conn, cookie, NULL);

  if (!wa || wa->override_redirect) {
    xfree(wa);
    return;
  }
  xfree(wa);
  if (!c) {
    cln_manage(e->window);
    xcb_flush(conn);
  }
}

void on_configure_request(xcb_generic_event_t *ge)
{
  xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ge;
  client_t *c;
  int x, y, w, h;

  if ((c = win_to_cln(e->window)) && c->isfloating) {
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
      cln_move(c, x, y);
    if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
      w = e->width;
    if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
      h = e->height;
    if (e->value_mask & (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))
      cln_resize(c, w, h);
  } else if (!c)
    grant_configure_request(e);
  xcb_flush(conn);
}

monitor_t *mon_create(void)
{
  monitor_t *m;

  m = xmalloc(sizeof(monitor_t));
  m->next = NULL;
  m->cln = NULL;
  m->np = LENGTH(pages);
  m->fp = 0;
  m->lx = 0;
  m->ly = 0;
  m->lw = scr->width_in_pixels - m->lx;
  m->lh = scr->height_in_pixels - m->ly;
  return m;
}

void mon_delete(monitor_t *m)
{
  // xassert(!m->cln, "deleting a monitor with clients");
  xfree(m);
}

// TODO: arranging non-focus monitor will yield bugs since INPAGE references the focus monitor,
//       doesn't matter fow now but remember to update this once we have multi-monitor support
void mon_arrange(monitor_t *m)
{
  layout_arg_t arg;
  client_t *c;

  arg.mon = m;
  arg.par = pages[m->fp].par;
  arg.ntiled = 0;
  for (c = next_inpage(m->cln); c; c = next_inpage(c->next)) {
    // if no client has focus, focus the first one in page
    if (!fc)
      cln_set_focus(c);
    // due to the tagging mechanism, there can be multiple clients in the same page
    // that wishes fullscreen, only the first one is granted fullscreen and focus
    if (c->isfullscr) {
      cln_set_fullscr(c, true);
      cln_set_focus(c);
      xcb_flush(conn);
      return;
    }
    // restack and count tiled clients in focus page
    if (c->isfloating)
      win_stack(c->frame, Top);
    else {
      win_stack(c->frame, Bottom);
      arg.ntiled++;
    }
  }

  // the layout may modify page parameters as they see fit
  if (arg.ntiled > 0)
    pages[m->fp].lt->fn(&arg);
  xcb_flush(conn);
  log("arranged page %s with %s", pages[m->fp].sym, pages[m->fp].lt->sym);
}

client_t *cln_create()
{
  xassert(fm, "no focus monitor");
  client_t *c;

  c = xmalloc(sizeof(client_t));
  c->next = NULL;
  c->tab = xmalloc(sizeof(xcb_window_t));
  c->tag = LSB(fm->fp);
  c->sel = 0;
  c->tcap = 1;
  c->nt = 0;
  c->ft = 0;
  c->isfloating = false;
  c->isfullscr = false;
  c->x = c->px = 0;
  c->y = c->py = 0;
  c->w = c->pw = VXWM_CLN_MIN_W;
  c->h = c->ph = VXWM_CLN_MIN_H;

  cln_add_frame(c);
  xcb_map_window(conn, c->frame);
  log("created client frame: %d", c->frame);
  return c;
}

void cln_manage(xcb_window_t win)
{
  client_t *c;
  xcb_atom_t win_type;

  c = cln_create();
  tab_attach(c, win);
  cln_attach(c);

  vals[0] = VXWM_WIN_EVENT_MASK;
  xcb_change_window_attributes(conn, win, XCB_CW_EVENT_MASK, vals);

  win_type = get_atom_prop(c->tab[c->ft], net_atom[NetWmWindowType]);
  if (win_type == net_atom[NetWmWindowTypeDialog])
    c->isfloating = true;

  mon_arrange(fm);
  xcb_map_window(conn, win);
  cln_set_focus(c);
}

void cln_delete(client_t *c)
{
  xcb_unmap_window(conn, c->frame);
  xcb_destroy_window(conn, c->frame);
  xcb_flush(conn);
  log("destroy frame: %d", c->frame);
  xfree(c->tab);
  xfree(c);
}

void cln_unmanage(client_t *c)
{
  xassert(c->nt == 0, "should not unmanage client with existing tabs");

  cln_set_focus(NULL);
  cln_detach(c);
  cln_delete(c);
  mon_arrange(fm);
}

void cln_set_border(client_t *c, int width)
{
  masks = XCB_CONFIG_WINDOW_BORDER_WIDTH;
  xcb_configure_window(conn, c->frame, masks, &width);
}

void cln_add_frame(client_t *c)
{
  uint32_t bw = 3, bp = VXWM_CLN_NORMAL_CLR;
  int i, n;

  c->frame = xcb_generate_id(conn);
  xcb_create_window(conn, scr->root_depth,
                    c->frame, root, 0, 0, 1, 1, bw,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    scr->root_visual, XCB_CW_BORDER_PIXEL, &bp);

  vals[0] = VXWM_FRAME_EVENT_MASK;
  xcb_change_window_attributes(conn, c->frame, XCB_CW_EVENT_MASK, vals);

  cln_set_border(c, VXWM_CLN_BORDER_W);

  masks = XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_BUTTON_RELEASE |
          XCB_EVENT_MASK_BUTTON_MOTION;
  for (i = 0, n = LENGTH(btnbinds); i < n; i++)
    xcb_grab_button(conn, 0, c->frame, masks, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                    XCB_NONE, XCB_NONE, btnbinds[i].btn, btnbinds[i].mod);
}

void cln_attach(client_t *c)
{
  c->next = fm->cln;
  fm->cln = c;
}

void cln_detach(client_t *c)
{
  client_t **cc;
  for (cc = &fm->cln; *cc && *cc != c; cc = &(*cc)->next) ;
  *cc = c->next;
}

void cln_set_focus(client_t *c)
{
  uint32_t fclr = VXWM_CLN_FOCUS_CLR;
  uint32_t nclr = VXWM_CLN_NORMAL_CLR;
  client_t *pf = NULL;

  if (fc == c)
    return;

  // update old focus client
  if (fc && fc->nt > 0) {
    pf = fc;
    fc = c;
    xcb_change_window_attributes(conn, pf->frame, XCB_CW_BORDER_PIXEL, &nclr);
    tab_draw(pf);
  }

  // assign new focus client or loose focus
  if ((fc = c)) {
    cln_raise(fc); // TODO: raise_on_focus option
    xcb_change_window_attributes(conn, fc->frame, XCB_CW_BORDER_PIXEL, &fclr);
    win_focus(fc->tab[fc->ft]);
    tab_draw(fc);
    log("new focus: %d", fc->frame);
  } else {
    win_focus(root);
    log("loosing focus");
  }
}

void cln_raise(client_t *c)
{
  win_stack(c->frame, Top);
  tab_draw(c);
}

void cln_set_fullscr(client_t *c, bool fullscr)
{
  if (fullscr) {
    win_stack(c->frame, Top);
    cln_set_border(c, 0);
    cln_move_resize(c, 0, 0, scr->width_in_pixels, scr->height_in_pixels);
  } else {
    cln_set_border(c, VXWM_CLN_BORDER_W);
    cln_move_resize(c, c->px, c->py, c->pw, c->ph);
  }
}

void tab_attach(client_t *c, xcb_window_t win)
{
  if (c->nt == c->tcap) {
    c->tcap <<= 1;
    c->tab = xrealloc(c->tab, sizeof(xcb_window_t) * c->tcap);
  }
  c->tab[c->nt++] = win;
  xcb_reparent_window(conn, win, c->frame, 0, VXWM_TAB_HEIGHT);
}

void tab_detach(client_t *c, int i)
{
  xassert(c && i < c->nt, "bad call to tab_detach");
  uint32_t lsb;

  if (c->sel & LSB(i))
    ns--;
  
  // preserve selection mask
  lsb = LSB(i) - 1;
  c->sel = ((c->sel >> 1) & ~lsb) + (c->sel & lsb);

  for (; i < c->nt - 1; i++)
    c->tab[i] = c->tab[i + 1];
  c->nt--;
  c->ft = MAX(c->ft - 1, 0);
}

void tab_draw(client_t *c)
{
  xassert(c && c->nt > 0, "bad call to tab_draw");
  int tw = c->w / c->nt, sw = VXWM_TAB_HEIGHT / 2;
  int i;

  draw_rect(0, 0, c->w, VXWM_TAB_HEIGHT, VXWM_TAB_NORMAL_CLR);
  if (c == fc)
    draw_rect(tw * c->ft, 0, tw, VXWM_TAB_HEIGHT, VXWM_TAB_FOCUS_CLR);
  for (i = 0; i < c->nt; i++)
    if (c->sel & LSB(i))
      draw_rect((i + 0.25) * tw, sw / 2, tw / 2, sw, VXWM_TAB_SELECT_CLR);
  draw_copy(c->frame, 0, 0, c->w, VXWM_TAB_HEIGHT);
}

void tab_kill(xcb_window_t win)
{
  if (has_proto(win, wm_atom[WmDeleteWindow]))
    send_msg(win, wm_atom[WmDeleteWindow]);
  else
    xcb_kill_client(conn, win);
}

void cln_move(client_t *c, int x, int y)
{
  c->px = c->x;
  c->py = c->y;
  masks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
  vals[0] = c->x = x;
  vals[1] = c->y = y;
  xcb_configure_window(conn, c->frame, masks, vals);
}

void cln_resize(client_t *c, int w, int h)
{
  if (w < VXWM_CLN_MIN_W || h < VXWM_CLN_MIN_H)
    return;
  c->pw = c->w;
  c->ph = c->h;
  masks = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  vals[0] = c->w = w;
  vals[1] = c->h = h;
  xcb_configure_window(conn, c->frame, masks, vals);
  xcb_configure_window(conn, c->tab[c->ft], masks, vals);
  tab_draw(c);
}

void cln_move_resize(client_t *c, int x, int y, int w, int h)
{
  cln_move(c, x, y);
  cln_resize(c, w, h);
}

void cln_show_hide(monitor_t *m)
{
  client_t *c;

  for (c = m->cln; c; c = c->next)
    if (INPAGE(c))
      cln_move_resize(c, c->x, c->y, c->w, c->h);
    else
      cln_move(c, scr->width_in_pixels, 0);
}

client_t *next_inpage(client_t *c)
{
  for (; c && !INPAGE(c); c = c->next) ;
  return c;
}

client_t *prev_inpage(client_t *c)
{
  client_t *p = next_inpage(fm->cln), *n;
  for (; p && (n = next_inpage(p->next)) != c; p = n) ;
  return p;
}

client_t *next_tiled(client_t *c)
{
  for (; c && (!INPAGE(c) || c->isfloating); c = c->next) ;
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
    for (i = 0; i < c->nt; i++)
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
      for (i = 0; i < c->nt; i++) if (c->sel & LSB(i))
        tab_kill(c->tab[i]);
  } else
    tab_kill(fc->tab[fc->ft]);
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

  if (!fc || fc->isfloating || !next_tiled(c->next))
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
  cln_detach(fc);
  if (p) {
    fc->next = p->next;
    p->next = fc;
  } else {
    fc->next = fm->cln;
    fm->cln = fc;
  }
  mon_arrange(fm);
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

  fc->sel ^= LSB(fc->ft);
  ns += (fc->sel & LSB(fc->ft)) ? +1 : -1;
  tab_draw(fc);
  xcb_flush(conn);
}

void bn_toggle_float(const arg_t *arg)
{
  UNUSED(arg);

  if (!fc)
    return;

  fc->isfloating = !fc->isfloating;
  mon_arrange(fm);
}

void bn_toggle_fullscr(const arg_t *arg)
{
  UNUSED(arg);

  if (!fc)
    return;

  if (fc->isfullscr) {
    cln_set_fullscr(fc, false);
    fc->isfullscr = false;
  } else
    fc->isfullscr = true;
  mon_arrange(fm);
}

void bn_merge_cln(const arg_t *arg)
{
  UNUSED(arg);
  client_t *c, *d, *mc;
  xcb_window_t win;
  int i;

  // maximum tab count is capped at 64
  if (!fc || ns == 0 || fc->nt + ns > 64)
    return;

  // determine merge destination
  switch (arg->t) {
    case This:
      mc = fc;
      break;
    default:
      mc = cln_create();
      cln_attach(mc);
      break;
  }
 
  c = next_selected(fm->cln);
  while (c) {
    while (c->sel) {
      for (i = 0; !(c->sel & LSB(i)); i++) ;
      win = c->tab[i];
      tab_detach(c, i);
      tab_attach(mc, win);
    }
    if (c->nt == 0) {
      d = c;
      c = next_selected(c->next);
      cln_detach(d);  
      cln_delete(d);
    } else {
      tab_draw(c);
      c = next_selected(c->next);
    }
  }
  xassert(ns == 0, "bad selection counting");
  win_stack(mc->tab[mc->ft], Top);
  cln_set_focus(mc);
  mon_arrange(fm);
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
    sc = cln_create();
    cln_attach(sc);
    win = fc->tab[fc->ft];
    tab_detach(fc, fc->ft);
    tab_attach(sc, win);
  } else for (c = next_selected(fm->cln); c; c = next_selected(c->next))
    while (c->sel) { // consume selection
      if (c->nt == 1) {
        ns--;
        c->sel = 0;
        break;
      }
      sc = cln_create();
      cln_attach(sc);
      for (i = 0; !(c->sel & LSB(i)); i++) ;
      win = c->tab[i];
      tab_detach(c, i);
      tab_attach(sc, win);
    }
  xassert(ns == 0, "bad selection counting");
  cln_set_focus(sc ? sc : fc);
  mon_arrange(fm);
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
  cln_set_focus(c);
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
  win_stack(win, Top);
  win_focus(win);
  tab_draw(fc);

  xcb_flush(conn);
  log("focusing tab %d (%d/%d)", win, fc->ft + 1, fc->nt);
}

void bn_focus_page(const arg_t *arg)
{
  client_t *c;

  if (fm->fp == arg->i)
    return;
  
  // lose selection when switching pages
  for (c = next_inpage(fm->cln); c; c = next_inpage(c->next))
    c->sel = 0;
  ns = 0;

  log("focus page %s", pages[arg->i].sym);
  fm->fp = arg->i;
  cln_set_focus(NULL);
  cln_show_hide(fm);
  mon_arrange(fm);
}

void bn_set_tag(const arg_t *arg)
{
  uint32_t page_tag = arg->u32;

  if (!fc || page_tag == 0)
    return;
  fc->tag = page_tag;
  cln_set_focus(NULL);
  cln_show_hide(fm);
  mon_arrange(fm);
}

void bn_set_param(const arg_t *arg)
{
  int *v = (int *)arg->v;
  
  pages[fm->fp].par[v[0]] += v[1];
  mon_arrange(fm);
}

void bn_set_layout(const arg_t *arg)
{
  xassert(arg->i < (int)LENGTH(layouts), "bad call to bn_set_layout");
  pages[fm->fp].lt = &layouts[arg->i];
  mon_arrange(fm);
}

// column layout
//   arrange clients in columns
//   parameter 0: number of columns
//   excess clients are piled in the right-most column
void column(const layout_arg_t *arg)
{
  int i, h, n = arg->ntiled, cols, colw;
  monitor_t *m = arg->mon;
  client_t *c;

  arg->par[0] = MAX(arg->par[0], 1);
  cols = MIN(arg->par[0], n);
  colw = m->lw / cols;
  h = m->lh / (n-cols+1);

  for (c = next_tiled(arg->mon->cln), i = 0; c; c = next_tiled(c->next), i++)
    if (i < cols - 1)
      cln_move_resize(c, m->lx + i * colw, m->ly, colw - BORDER, m->lh - BORDER);
    else
      cln_move_resize(c, m->lx + m->lw - colw, m->ly + h * (i-cols+1), colw - BORDER, h - BORDER);
}

// stack layout
//   arrange clients in two stacks
//   parameter 0: number of clients in left stack
//   excess clients are piled in right stack 
void stack(const layout_arg_t *arg)
{
  int i, ln, lw, rn, rw, n = arg->ntiled;
  monitor_t *m = arg->mon;
  client_t *c;

  arg->par[0] = MAX(arg->par[0], 1);
  ln = MIN(arg->par[0], n);
  rn = n - ln;
  if (n <= ln) {
    lw = m->lw;
    rw = 0;
  } else
    lw = rw = m->lw / 2;

  for (c = next_tiled(arg->mon->cln), i = 0; c; c = next_tiled(c->next), i++)
    if (i < ln)
      cln_move_resize(c, m->lx, m->ly + m->lh / ln * i, lw - BORDER, m->lh / ln - BORDER);
    else
      cln_move_resize(c, m->lx + lw, m->ly + m->lh / rn * (i-ln), rw - BORDER, m->lh / rn - BORDER);
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
