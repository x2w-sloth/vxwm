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
#include "util.h"
#include "win.h"
#include "draw.h"

#define VXWM_CLN_MIN_W           30
#define VXWM_CLN_MIN_H           30
#define VXWM_TAB_NAME_BUF        128
#define VXWM_ROOT_NAME_BUF       128
#define VXWM_LT_STATUS_BUF       32
#define BORDER                   2 * VXWM_CLN_BORDER_W
#define INPAGE(C)                (C->tag & 1 << fm->fp)

#define VXWM_ROOT_EVENT_MASK    (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |\
                                 XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |\
                                 XCB_EVENT_MASK_PROPERTY_CHANGE)

#define VXWM_FRAME_EVENT_MASK   (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |\
                                 XCB_EVENT_MASK_EXPOSURE |\
                                 XCB_EVENT_MASK_ENTER_WINDOW)

#define VXWM_WIN_EVENT_MASK     (XCB_EVENT_MASK_STRUCTURE_NOTIFY |\
                                 XCB_EVENT_MASK_FOCUS_CHANGE)

// a monitor corresponds to a physical display and contains pages
struct monitor {
  monitor_t *next;     // monitor linked list
  client_t *cln;       // clients under this monitor
  int np, fp;          // number of pages, index of focus page
  int lx, ly, lw, lh;  // layout space where tiled clients are arranged in
  xcb_window_t barwin; // status bar window
  int barh;            // status bar height
  char lt_status[VXWM_LT_STATUS_BUF]; // layout status buffer
};

// a page displays a subset of clients under a layout policy
struct page {
  const char *sym;     // page identifier string
  layout_t lt;         // layout policy for this page
  int par[3];          // layout parameters
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
  char (*name)[VXWM_TAB_NAME_BUF]; // name buffer for tabs
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

static void args(int, char **);
static void setup(void);
static void scan(void);
static void run(void);
static void cleanup(void);
static void atom_setup(void);
static xcb_keycode_t *keysym_to_keycodes(xcb_keysym_t);
static xcb_keysym_t keycode_to_keysym(xcb_keycode_t);
static void grab_keys(void);
static void ptr_motion(ptr_state_t);
static void grant_configure_request(xcb_configure_request_event_t *);
static void on_key_press(xcb_generic_event_t *);
static void on_button_press(xcb_generic_event_t *);
static void on_motion_notify(xcb_generic_event_t *);
static void on_enter_notify(xcb_generic_event_t *);
static void on_focus_in(xcb_generic_event_t *);
static void on_expose(xcb_generic_event_t *);
static void on_destroy_notify(xcb_generic_event_t *);
static void on_unmap_notify(xcb_generic_event_t *);
static void on_map_request(xcb_generic_event_t *);
static void on_configure_request(xcb_generic_event_t *);
static void on_property_notify(xcb_generic_event_t *);
static monitor_t *mon_create(void);
static void mon_delete(monitor_t *);
static void mon_arrange(monitor_t *);
static void mon_draw_bar(monitor_t *);
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
static void cln_set_tag(client_t *, uint32_t, bool);
static void cln_attach_tab(client_t *, xcb_window_t);
static void cln_detach_tab(client_t *, xcb_window_t);
static void cln_draw_tabs(client_t *);
static client_t *next_inpage(client_t *);
static client_t *prev_inpage(client_t *);
static client_t *next_tiled(client_t *);
static client_t *next_selected(client_t *);
static client_t *cln_from_tab(xcb_window_t);
static client_t *cln_from_frame(xcb_window_t);
static client_t *cln_focus_fallback(client_t *);
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
static void bn_toggle_tag(const arg_t *);
static void bn_set_tag(const arg_t *);
static void bn_set_param(const arg_t *);
static void bn_set_layout(const arg_t *);
static void column(const layout_arg_t *);
static void stack(const layout_arg_t *);

static xcb_key_symbols_t *symbols;
static monitor_t *fm;
static client_t *fc;
static ptr_state_t ptr_state;
static bool ptr_first_motion;
static bool running;
static int ptr_x, ptr_y;
static int win_x, win_y, win_w, win_h;
static int nsel;
static int barh;
static uint32_t vals[8], masks;
static char root_name[VXWM_ROOT_NAME_BUF];
static const handler_t handler[XCB_NO_OPERATION] = {
  [XCB_KEY_PRESS] = on_key_press,
  [XCB_BUTTON_PRESS] = on_button_press,
  [XCB_MOTION_NOTIFY] = on_motion_notify,
  [XCB_ENTER_NOTIFY] = on_enter_notify,
  [XCB_FOCUS_IN] = on_focus_in,
  [XCB_EXPOSE] = on_expose,
  [XCB_DESTROY_NOTIFY] = on_destroy_notify,
  [XCB_UNMAP_NOTIFY] = on_unmap_notify,
  [XCB_MAP_REQUEST] = on_map_request,
  [XCB_CONFIGURE_REQUEST] = on_configure_request,
  [XCB_PROPERTY_NOTIFY] = on_property_notify,
//[XCB_CLIENT_MESSAGE]  = on_client_message,
//[XCB_MAPPING_NOTIFY]  = on_mapping_notify,
};
session_t sn; // global session instance

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
  sn.conn = xcb_connect(NULL, NULL);
  if (!sn.conn || xcb_connection_has_error(sn.conn))
    die("failed to connect to x server\n");
  sn.scr = xcb_setup_roots_iterator(xcb_get_setup(sn.conn)).data;
  if (!sn.scr)
    die("failed to capture screen\n");

  // configure root window, check if another wm is running
  sn.root = sn.scr->root;
  vals[0] = VXWM_ROOT_EVENT_MASK;
  cookie = xcb_change_window_attributes_checked(sn.conn, sn.root, XCB_CW_EVENT_MASK, vals);
  error = xcb_request_check(sn.conn, cookie);
  xcb_flush(sn.conn);
  if (error)
    die("another window manager is running\n");
  LOGV("configured root window %d\n", sn.root)

  // initialize drawing context and atoms, setup monitor
  draw_setup();
  draw_select_font(VXWM_FONT, VXWM_FONT_SIZE, &barh);
  atom_setup();
  fm = mon_create();

  // load symbols and grab keys on root window
  symbols = xcb_key_symbols_alloc(sn.conn);
  if (!symbols)
    die("failed to allocate key symbol table\n");
  grab_keys();
  xcb_flush(sn.conn);

  // initialize status globals
  strncpy(root_name, "vxwm "VXWM_VERSION, VXWM_ROOT_NAME_BUF);
  ptr_state = PtrUngrabbed;
  fc = NULL;
  nsel = 0;
  running = true;
}

void scan(void)
{
  xcb_query_tree_cookie_t qtc;
  xcb_query_tree_reply_t *qtr;
  xcb_window_t *win;
  uint8_t map_state;
  bool override_redirect;
  int i, nwin;

  qtc = xcb_query_tree(sn.conn, sn.root);
  qtr = xcb_query_tree_reply(sn.conn, qtc, NULL);
  if (!qtr || !(win = xcb_query_tree_children(qtr)))
    return;
  nwin = xcb_query_tree_children_length(qtr);
  for (i = 0; i < nwin; i++) {
    LOGV("scanning %d\n", win[i])
    win_get_attr(win[i], &override_redirect, &map_state);
    if (override_redirect || map_state == XCB_MAP_STATE_UNMAPPED)
      continue;
    cln_manage(win[i]);
  }
  xfree(qtr);
  xcb_flush(sn.conn);
}

void run(void)
{
  xcb_generic_event_t *ge;
  uint8_t type;

  xcb_flush(sn.conn);
  while (running && (ge = xcb_wait_for_event(sn.conn))) {
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
  if (sn.conn && !xcb_connection_has_error(sn.conn))
    xcb_disconnect(sn.conn);
}


#define ATOM(NAME) xcb_intern_atom(sn.conn, false, (uint16_t)strlen(NAME), NAME)
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
  wm_cookies[WmState]        = ATOM("WM_STATE");
  net_cookies[NetWmName]     = ATOM("_NET_WM_NAME");
  net_cookies[NetWmWindowType] = ATOM("_NET_WM_WINDOW_TYPE");
  net_cookies[NetWmWindowTypeDialog] = ATOM("_NET_WM_WINDOW_TYPE_DIALOG");

  for (i = 0; i < WmAtomsLast; i++)
    if ((reply = xcb_intern_atom_reply(sn.conn, wm_cookies[i], NULL))) {
      sn.wm_atom[i] = reply->atom;
      xfree(reply);
    }

  for (i = 0; i < NetAtomsLast; i++)
    if ((reply = xcb_intern_atom_reply(sn.conn, net_cookies[i], NULL))) {
      sn.net_atom[i] = reply->atom;
      xfree(reply);
    }
}
#undef ATOM

xcb_keycode_t *keysym_to_keycodes(xcb_keysym_t keysym)
{
  xcb_keycode_t *keycodes;

  keycodes = xcb_key_symbols_get_keycode(symbols, keysym); // this can be slow
  assert(keycodes && "failed to allocate keycodes from symbol table");
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

  xcb_ungrab_key(sn.conn, XCB_GRAB_ANY, sn.root, XCB_MOD_MASK_ANY);
  for (i = 0, n = LENGTH(keybinds); i < n; i++) {
    keycodes = keysym_to_keycodes(keybinds[i].sym);
    for (j = 0; keycodes[j] != XCB_NO_SYMBOL; j++)
      xcb_grab_key(sn.conn, 1, sn.root, keybinds[i].mod, keycodes[j],
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xfree(keycodes);
  }
}

void ptr_motion(ptr_state_t state)
{
  xcb_query_pointer_reply_t *ptr;

  ptr = xcb_query_pointer_reply(sn.conn, xcb_query_pointer(sn.conn, sn.root), NULL);
  assert(ptr && "did not receive a reply from query pointer");
  ptr_x = ptr->root_x;
  ptr_y = ptr->root_y;
  xfree(ptr);

  win_get_geometry(fc->frame, &win_x, &win_y, &win_w, &win_h, NULL);

  ptr_state = state; // see on_motion_notify
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
  xcb_configure_window(sn.conn, e->window, masks, vals);
}

void on_key_press(xcb_generic_event_t *ge)
{
  xcb_key_press_event_t *e = (xcb_key_press_event_t *)ge;
  xcb_keysym_t keysym = keycode_to_keysym(e->detail);
  int i, n;
  LOGV("on_key_press: %d @ %d\n", e->event, e->sequence)

  for (i = 0, n = LENGTH(keybinds); i < n; i++)
    if (keysym == keybinds[i].sym && e->state == keybinds[i].mod && keybinds[i].fn)
      keybinds[i].fn(&keybinds[i].arg);
}

void on_button_press(xcb_generic_event_t *ge)
{
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)ge;
  int i, n;
  LOGV("on_button_press: %d @ %d\n", e->event, e->sequence)

  cln_set_focus(cln_from_frame(e->event));
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
  xcb_flush(sn.conn);
}

void on_enter_notify(xcb_generic_event_t *ge)
{
  xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ge;
  client_t *c;

  if (e->mode != XCB_NOTIFY_MODE_NORMAL)
    return;
  if (e->detail == XCB_NOTIFY_DETAIL_ANCESTOR || e->detail == XCB_NOTIFY_DETAIL_VIRTUAL
  ||  e->detail == XCB_NOTIFY_DETAIL_INFERIOR)
    return;
  LOGV("on_enter_notify: %d, %d @ %d\n", e->event, e->detail, e->sequence)

  if ((c = cln_from_frame(e->event))) {
    cln_set_focus(c);
    xcb_flush(sn.conn);
  }
}

void on_focus_in(xcb_generic_event_t *ge)
{
  xcb_focus_in_event_t *e = (xcb_focus_in_event_t *)ge;
  client_t *c;

  // we are only interested in mode Normal and WhileGrabbed
  if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB)
    return;
  if (e->detail == XCB_NOTIFY_DETAIL_POINTER)
    return;
  LOGV("on_focus_in: %d @ %d\n", e->event, e->sequence)

  if ((c = cln_from_tab(e->event))) {
    cln_set_focus(c);
    mon_draw_bar(fm);
  }
}

void on_expose(xcb_generic_event_t *ge)
{
  xcb_expose_event_t *e = (xcb_expose_event_t *)ge;
  client_t *c;

  if (e->window == fm->barwin)
    mon_draw_bar(fm);
  else if ((c = cln_from_frame(e->window)))
    cln_draw_tabs(c);
}

void on_destroy_notify(xcb_generic_event_t *ge)
{
  xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ge;
  client_t *c = cln_from_tab(e->window);
  arg_t arg = { .p = This };
  LOGV("on_destroy_notify: %d\n", e->window)

  if (!c)
    return;

  cln_detach_tab(c, e->window);
  if (c->nt == 0)
    cln_unmanage(c);
  else
    bn_focus_tab(&arg);
}

void on_unmap_notify(xcb_generic_event_t *ge)
{
  xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ge;
  client_t *c;
  arg_t arg = { .p = This };
  LOGV("on_unmap_notify: %d\n", e->window)

  if ((c = cln_from_tab(e->window))) {
    cln_detach_tab(c, e->window);
    win_set_state(e->window, XCB_ICCCM_WM_STATE_WITHDRAWN);
    xcb_change_save_set(sn.conn, XCB_SET_MODE_DELETE, e->window);
    LOGI("window %d deleted from save set\n", e->window)
    if (c->nt == 0)
      cln_unmanage(c);
    else
      bn_focus_tab(&arg);
  }
}

void on_map_request(xcb_generic_event_t *ge)
{
  xcb_map_request_event_t *e = (xcb_map_request_event_t *)ge;
  client_t *c;
  bool override_redirect;
  LOGV("on_map_request: %d\n", e->window)

  win_get_attr(e->window, &override_redirect, NULL);

  if (override_redirect)
    return;
  if (!(c = cln_from_tab(e->window))) {
    cln_manage(e->window);
    xcb_flush(sn.conn);
  }
}

void on_configure_request(xcb_generic_event_t *ge)
{
  xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ge;
  client_t *c;
  int x, y, w, h;
  LOGV("on_configure_request: %d\n", e->window)

  if ((c = cln_from_tab(e->window)) && c->isfloating) {
    assert(e->window == c->tab[c->ft] && "configured window is not focus tab");
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
  xcb_flush(sn.conn);
}

void on_property_notify(xcb_generic_event_t *ge)
{
  xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)ge;
  LOGV("on_property_notify: %d @ %d\n", e->window, e->sequence)

  if (e->window == sn.root && e->atom == XCB_ATOM_WM_NAME) {
    win_get_text_prop(sn.root, XCB_ATOM_WM_NAME, root_name, VXWM_ROOT_NAME_BUF);
    mon_draw_bar(fm);
  }
  xcb_flush(sn.conn);
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
  m->lw = sn.scr->width_in_pixels - m->lx;
  m->lh = sn.scr->height_in_pixels - m->ly - barh;
  m->barwin = xcb_generate_id(sn.conn);
  m->barh = barh;
  memset(m->lt_status, 0, VXWM_LT_STATUS_BUF);

  masks = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
  vals[0] = 1;
  vals[1] = XCB_EVENT_MASK_EXPOSURE;
  xcb_create_window(sn.conn, sn.scr->root_depth, m->barwin, sn.root,
                    0, m->lh, sn.scr->width_in_pixels, barh, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, sn.scr->root_visual, masks, vals);
  xcb_map_window(sn.conn, m->barwin);
  return m;
}

void mon_delete(monitor_t *m)
{
  // xassert(!m->cln && "deleting a monitor with clients");
  xcb_unmap_window(sn.conn, m->barwin);
  xcb_destroy_window(sn.conn, m->barwin);
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
    // due to the tagging mechanism, there can be multiple clients in the same page
    // that wishes fullscreen, only the first one is granted fullscreen and focus
    if (c->isfullscr) {
      cln_set_fullscr(c, true);
      cln_set_focus(c);
      xcb_flush(sn.conn);
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
  memset(m->lt_status, 0, VXWM_LT_STATUS_BUF);
  if (arg.ntiled > 0)
    pages[m->fp].lt(&arg);
  xcb_flush(sn.conn);
  LOGI("arranged page %s\n", pages[m->fp].sym)
}

void mon_draw_bar(monitor_t *m)
{
  const color_t fg = 0xCCCCCC;
  const color_t bg = 0x333333;
  int i, n, x, tw, pad = 28, sw = sn.scr->width_in_pixels;
  LOGV("drawing status bar\n")

  draw_rect_filled(0, 0, sw, fm->barh, bg);

  // draw page symbols
  n = LENGTH(pages);
  for (i = 0, x = 0; i < n; i++, x += tw) {
    draw_text_extents(pages[i].sym, &tw, NULL);
    tw += pad;
    if (i == m->fp) {
      draw_rect_filled(x, 0, tw, fm->barh, fg);
      draw_text(x, 0, tw, fm->barh, pages[i].sym, bg, pad / 2);
    } else {
      draw_text(x, 0, tw, fm->barh, pages[i].sym, fg, pad / 2);
      if (fc && (LSB(i) & fc->tag))
        draw_rect_filled(x + 5, 5, 5, 5, fg);
    }
  }
  pad = 8;

  // draw layout status
  if (m->lt_status[0]) {
    draw_text_extents(m->lt_status, &tw, NULL);
    tw += pad;
    draw_text(x, 0, tw, fm->barh, m->lt_status, fg, pad / 2);
    x += tw;
  }

  // draw focus tab title
  if (fc) {
    x = MAX(x, sw / 2);
    draw_text_extents(fc->name[fc->ft], &tw, NULL);
    draw_text(x - tw / 2, 0, tw, fm->barh, fc->name[fc->ft], fg, 0);
  }

  // draw root window title
  draw_text_extents(root_name, &tw, NULL);
  tw += pad;
  x = sn.scr->width_in_pixels - tw;
  draw_rect_filled(x, 0, tw, fm->barh, fg);
  draw_arc_filled(x, fm->barh/2, fm->barh/2., 90, 270, fg);
  draw_text(x, 0, tw, fm->barh, root_name, bg, pad / 2);

  draw_copy(m->barwin, 0, 0, sn.scr->width_in_pixels, fm->barh);
}

client_t *cln_create()
{
  assert(fm && "no focus monitor");
  client_t *c;

  c = xmalloc(sizeof(client_t));
  c->next = NULL;
  c->tab = xmalloc(sizeof(xcb_window_t));
  c->name = xmalloc(VXWM_TAB_NAME_BUF);
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
  memset(c->name, 0, VXWM_TAB_NAME_BUF);

  cln_add_frame(c);
  xcb_map_window(sn.conn, c->frame);
  LOGI("created client frame: %d\n", c->frame)
  return c;
}

void cln_manage(xcb_window_t win)
{
  client_t *c;
  xcb_atom_t win_type;
  int x, y, w, h;

  xcb_change_save_set(sn.conn, XCB_SET_MODE_INSERT, win);
  LOGI("window %d added to save set\n", win)

  c = cln_create();
  cln_attach_tab(c, win);
  cln_attach(c);

  xcb_map_window(sn.conn, win);
  if (win_get_atom_prop(c->tab[c->ft], sn.net_atom[NetWmWindowType], &win_type)
  &&  win_type == sn.net_atom[NetWmWindowTypeDialog])
    c->isfloating = true;

  if (c->isfloating) {
    win_get_geometry(win, &x, &y, &w, &h, NULL);
    cln_move_resize(c, x, y, w, h);
  }

  win_set_state(win, XCB_ICCCM_WM_STATE_NORMAL);
  mon_arrange(fm);
  cln_set_focus(c);
  mon_draw_bar(fm);
}

void cln_delete(client_t *c)
{
  xcb_unmap_window(sn.conn, c->frame);
  xcb_destroy_window(sn.conn, c->frame);
  xcb_flush(sn.conn);
  LOGI("destroy frame: %d\n", c->frame)
  xfree(c->tab);
  xfree(c->name);
  xfree(c);
}

void cln_unmanage(client_t *c)
{
  assert(c->nt == 0 && "should not unmanage client with existing tabs");
  client_t *fb;

  fb = cln_focus_fallback(c);
  cln_detach(c);
  cln_delete(c);
  mon_arrange(fm);
  cln_set_focus(fb);
  mon_draw_bar(fm);
}

void cln_set_border(client_t *c, int width)
{
  masks = XCB_CONFIG_WINDOW_BORDER_WIDTH;
  xcb_configure_window(sn.conn, c->frame, masks, &width);
}

void cln_add_frame(client_t *c)
{
  uint32_t bw = 3, bp = VXWM_CLN_NORMAL_CLR;
  int i, n;

  c->frame = xcb_generate_id(sn.conn);
  xcb_create_window(sn.conn, sn.scr->root_depth,
                    c->frame, sn.root, 0, 0, 1, 1, bw,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                    sn.scr->root_visual, XCB_CW_BORDER_PIXEL, &bp);

  vals[0] = VXWM_FRAME_EVENT_MASK;
  xcb_change_window_attributes(sn.conn, c->frame, XCB_CW_EVENT_MASK, vals);

  cln_set_border(c, VXWM_CLN_BORDER_W);

  masks = XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_BUTTON_RELEASE |
          XCB_EVENT_MASK_BUTTON_MOTION;
  for (i = 0, n = LENGTH(btnbinds); i < n; i++)
    xcb_grab_button(sn.conn, 0, c->frame, masks, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
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
  xcb_generic_event_t *ge;
  uint8_t type;
  uint32_t fclr = VXWM_CLN_FOCUS_CLR;
  uint32_t nclr = VXWM_CLN_NORMAL_CLR;
  client_t *pf = NULL;

  if (fc == c)
    return;

  // update old focus client
  if (fc && fc->nt > 0) {
    pf = fc;
    fc = c;
    xcb_change_window_attributes(sn.conn, pf->frame, XCB_CW_BORDER_PIXEL, &nclr);
    cln_draw_tabs(pf);
  }

  // assign new focus client or loose focus
  if ((fc = c)) {
    cln_raise(fc);
    xcb_change_window_attributes(sn.conn, fc->frame, XCB_CW_BORDER_PIXEL, &fclr);
    win_focus(fc->tab[fc->ft]);
    cln_draw_tabs(fc);
    LOGI("new focus: %d (%d)\n", fc->tab[fc->ft], fc->frame)
  } else {
    win_focus(sn.root);
    LOGI("loosing focus\n")
  }

  // ignore remaining enter notify events in local event queue
  while ((ge = xcb_poll_for_queued_event(sn.conn))) {
    type = XCB_EVENT_RESPONSE_TYPE(ge);
    if (type != XCB_ENTER_NOTIFY && handler[type])
      handler[type](ge);
    xfree(ge);
  }
}

INLINE
void cln_raise(client_t *c)
{
  win_stack(c->frame, Top);
  cln_draw_tabs(c);
}

void cln_set_fullscr(client_t *c, bool fullscr)
{
  if (fullscr) {
    win_stack(c->frame, Top);
    cln_set_border(c, 0);
    cln_move_resize(c, 0, 0, sn.scr->width_in_pixels, sn.scr->height_in_pixels);
  } else {
    cln_set_border(c, VXWM_CLN_BORDER_W);
    cln_move_resize(c, c->px, c->py, c->pw, c->ph);
  }
}

void cln_attach_tab(client_t *c, xcb_window_t win)
{
  if (c->nt == c->tcap) {
    c->tcap <<= 1;
    c->tab = xrealloc(c->tab, sizeof(xcb_window_t) * c->tcap);
    c->name = xrealloc(c->name, VXWM_TAB_NAME_BUF * c->tcap);
  }

  c->tab[c->nt] = win;
  if (!win_get_text_prop(win, sn.net_atom[NetWmName], c->name[c->nt], VXWM_TAB_NAME_BUF))
    win_get_text_prop(win, XCB_ATOM_WM_NAME, c->name[c->nt], VXWM_TAB_NAME_BUF);
  c->nt++;

  // If the window is already mapped and has the structure notify event mask,
  // then reparenting generates an unmap notify on the reparented window.
  // We avoid that by first configuring the window to have no event masks.
  vals[0] = XCB_EVENT_MASK_NO_EVENT;
  xcb_change_window_attributes(sn.conn, win, XCB_CW_EVENT_MASK, vals);
  xcb_reparent_window(sn.conn, win, c->frame, 0, VXWM_TAB_HEIGHT);
  vals[0] = VXWM_WIN_EVENT_MASK;
  xcb_change_window_attributes(sn.conn, win, XCB_CW_EVENT_MASK, vals);
  LOGV("attached %d under frame %d\n", win, c->frame)
}

void cln_detach_tab(client_t *c, xcb_window_t win)
{
  uint32_t lsb_mask;
  int i;

  for (i = 0; i < c->nt && c->tab[i] != win; i++) ;
  if (i >= c->nt) {
    LOGW("window not found in cln_detach_tab\n")
    return;
  }

  // preserve selection mask
  if (c->sel & LSB(i))
    nsel--;
  lsb_mask = LSB(i) - 1;
  c->sel = ((c->sel >> 1) & ~lsb_mask) + (c->sel & lsb_mask);

  for (; i < c->nt - 1; i++)
    c->tab[i] = c->tab[i + 1];
  c->nt--;
  c->ft = MAX(c->ft - 1, 0);
}

void cln_draw_tabs(client_t *c)
{
  int tw, sw, i;

  if (!c || c->nt == 0)
    return;
  tw = c->w / c->nt;
  sw = VXWM_TAB_HEIGHT / 2;
  draw_rect_filled(0, 0, c->w, VXWM_TAB_HEIGHT, VXWM_TAB_NORMAL_CLR);
  if (c == fc)
    draw_rect_filled(tw * c->ft, 0, tw, VXWM_TAB_HEIGHT, VXWM_TAB_FOCUS_CLR);
  for (i = 0; i < c->nt; i++)
    if (c->sel & LSB(i))
      draw_rect_filled((i + 0.25) * tw, sw / 2, tw / 2, sw, VXWM_TAB_SELECT_CLR);
  draw_copy(c->frame, 0, 0, c->w, VXWM_TAB_HEIGHT);
}

void cln_move(client_t *c, int x, int y)
{
  c->px = c->x;
  c->py = c->y;
  masks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
  vals[0] = c->x = x;
  vals[1] = c->y = y;
  xcb_configure_window(sn.conn, c->frame, masks, vals);
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
  xcb_configure_window(sn.conn, c->frame, masks, vals);
  vals[1] = c->h - VXWM_TAB_HEIGHT;
  xcb_configure_window(sn.conn, c->tab[c->ft], masks, vals);
  cln_draw_tabs(c);
}

INLINE
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
    else {
      masks = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
      vals[0] = sn.scr->width_in_pixels;
      vals[1] = 0;
      xcb_configure_window(sn.conn, c->frame, masks, vals);
    }
}

void cln_set_tag(client_t *c, uint32_t tag, bool toggle)
{
  assert(c && "bad call to cln_set_tag");
  client_t *fb;

  if (toggle)
    tag ^= c->tag;
  if (tag == 0) {
    LOGW("ignoring request to set client tag to 0\n")
    return;
  }
  fb = cln_focus_fallback(c);
  c->tag = tag;
  if (!INPAGE(c)) {
    if (c->sel) {
      c->sel = 0;
      nsel--;
    }
    cln_show_hide(fm);
    mon_arrange(fm);
    cln_set_focus(fb);
  }
  mon_draw_bar(fm);
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

client_t *cln_from_tab(xcb_window_t win)
{
  client_t *c;
  int i;

  for (c = fm->cln; c; c = c->next)
    for (i = 0; i < c->nt; i++)
      if (c->tab[i] == win)
        return c;
  return NULL;
}

client_t *cln_from_frame(xcb_window_t frame)
{
  client_t *c;

  for (c = fm->cln; c; c = c->next)
    if (c->frame == frame)
      return c;
  return NULL;
}

client_t *cln_focus_fallback(client_t *c)
{
  client_t *fb;

  if (!(fb = next_inpage(c->next)))
    fb = prev_inpage(c);
  return fb;
}

void bn_quit(UNUSED const arg_t *arg)
{
  running = false;
}

void bn_spawn(const arg_t *arg)
{
  char **args = (char **)arg->v;
  pid_t pid = fork();

  if (pid == -1) {
    LOGW("fork failed\n")
  } else if (pid == 0) {
    setsid();
    execvp(*args, args);
    fprintf(stderr, "execvp %s ", *args);
    perror("failed");
    exit(EXIT_SUCCESS);
  }
}

void bn_kill_tab(UNUSED const arg_t *arg)
{
  client_t *c;
  int i;

  if (!fc)
    return;

  if (nsel > 0) { // consume selection
    for (c = next_selected(fm->cln); c; c = next_selected(c->next))
      for (i = 0; i < c->nt; i++) if (c->sel & LSB(i))
        win_kill(c->tab[i]);
  } else
    win_kill(fc->tab[fc->ft]);
  xcb_flush(sn.conn);
}

void bn_swap_tab(const arg_t *arg)
{
  char buf[VXWM_TAB_NAME_BUF];
  int swp = -1;

  if (!fc || fc->nt == 1)
    return;

  switch (arg->p) {
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
      assert(false && "bad argument in bn_swap_tab");
      return;
  }
  SWAP_BITS(fc->sel, swp, fc->ft);
  SWAP(fc->tab[swp], fc->tab[fc->ft])
  memcpy(buf, fc->name[swp], VXWM_TAB_NAME_BUF);
  memcpy(fc->name[swp], fc->name[fc->ft], VXWM_TAB_NAME_BUF);
  memcpy(fc->name[fc->ft], buf, VXWM_TAB_NAME_BUF);
  bn_focus_tab(arg);
}

void bn_swap_cln(const arg_t *arg)
{
  client_t *p = NULL, *c = next_tiled(fm->cln), *pf;

  if (!fc || fc->isfloating || !next_tiled(c->next))
    return;

  switch (arg->p) {
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
      assert(false && "bad argument in bn_swap_cln");
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
  // loose focus briefly so that enter notify does not steal focus after arrange
  pf = fc;
  cln_set_focus(NULL);
  mon_arrange(fm);
  cln_set_focus(pf);
}

void bn_move_cln(UNUSED const arg_t *arg)
{
  ptr_motion(PtrMoveCln);
}

void bn_resize_cln(UNUSED const arg_t *arg)
{
  ptr_motion(PtrResizeCln);
}

void bn_toggle_select(UNUSED const arg_t *arg)
{
  if (!fc)
    return;

  fc->sel ^= LSB(fc->ft);
  nsel += (fc->sel & LSB(fc->ft)) ? +1 : -1;
  cln_draw_tabs(fc);
  xcb_flush(sn.conn);
}

void bn_toggle_float(UNUSED const arg_t *arg)
{
  if (!fc)
    return;

  fc->isfloating = !fc->isfloating;
  mon_arrange(fm);
}

void bn_toggle_fullscr(UNUSED const arg_t *arg)
{
  client_t *pf = fc;

  if (!fc)
    return;

  if (fc->isfullscr) {
    cln_set_fullscr(fc, false);
    fc->isfullscr = false;
  } else
    fc->isfullscr = true;
  cln_set_focus(NULL);
  mon_arrange(fm);
  cln_set_focus(pf);
}

void bn_merge_cln(const arg_t *arg)
{
  client_t *c, *d, *mc;
  xcb_window_t win;
  int i;

  // maximum tab count is capped at 64
  if (!fc || nsel == 0 || fc->nt + nsel > 64) {
    LOGW("merging will result in client %d hosting over 64 tabs\n", fc->frame)
    return;
  }

  // determine merge destination
  switch (arg->p) {
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
    while (c->sel) { // consume selection
      for (i = 0; !(c->sel & LSB(i)); i++) ;
      win = c->tab[i];
      cln_detach_tab(c, win);
      cln_attach_tab(mc, win);
      win_set_state(win, XCB_ICCCM_WM_STATE_ICONIC);
    }
    if (c->nt == 0) {
      d = c;
      c = next_selected(c->next);
      cln_detach(d);  
      cln_delete(d);
    } else {
      cln_draw_tabs(c);
      c = next_selected(c->next);
    }
  }
  assert(nsel == 0 && "bad selection counting");
  win_stack(mc->tab[mc->ft], Top);
  win_set_state(mc->tab[mc->ft], XCB_ICCCM_WM_STATE_NORMAL);
  mon_arrange(fm);
  cln_set_focus(mc);
  xcb_flush(sn.conn);
}

void bn_split_cln(UNUSED const arg_t *arg)
{
  client_t *c, *sc = NULL;
  xcb_window_t win;
  int i;

  if (!fc || (nsel == 0 && fc->nt == 1))
    return;

  if (nsel == 0) { // split focus tab from focus client
    sc = cln_create();
    cln_attach(sc);
    win = fc->tab[fc->ft];
    cln_detach_tab(fc, win);
    cln_attach_tab(sc, win);
    win_set_state(fc->tab[fc->ft], XCB_ICCCM_WM_STATE_NORMAL);
  } else for (c = next_selected(fm->cln); c; c = next_selected(c->next)) {
    while (c->sel) { // consume selection
      if (c->nt == 1) {
        nsel--;
        c->sel = 0;
        break;
      }
      sc = cln_create();
      cln_attach(sc);
      for (i = 0; !(c->sel & LSB(i)); i++) ;
      win = c->tab[i];
      cln_detach_tab(c, win);
      cln_attach_tab(sc, win);
      win_set_state(win, XCB_ICCCM_WM_STATE_NORMAL);
    }
    win_set_state(c->tab[c->ft], XCB_ICCCM_WM_STATE_NORMAL);
  }
  assert(nsel == 0 && "bad selection counting");
  mon_arrange(fm);
  cln_set_focus(sc ? sc : fc);
  xcb_flush(sn.conn);
}

void bn_focus_cln(const arg_t *arg)
{
  client_t *c = NULL;

  // lock focus when fullscreen
  if (!fc || fc->isfullscr)
    return;

  switch (arg->p) {
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
  mon_draw_bar(fm);
}

void bn_focus_tab(const arg_t *arg)
{
  pos_t p = arg->p;
  xcb_window_t win, old;
  int n;

  if (!fc)
    return;

  old = fc->tab[fc->ft];
  n = fc->nt;
  switch (p) {
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
      fc->ft = (fc->ft + (int)p + n) % n;
      break;
  }
  win = fc->tab[fc->ft];

  // since ICCCM does not explicitly define what "IconicState" is,
  // vxwm considers tabs that are not focused to be "Iconic" since
  // these tab windows are unviewable but not withdrawn either.
  if (win != old) {
    win_set_state(old, XCB_ICCCM_WM_STATE_ICONIC);
    win_set_state(win, XCB_ICCCM_WM_STATE_NORMAL);
  }

  // resize tab window to match client dimensions
  masks = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
  vals[0] = fc->w;
  vals[1] = fc->h - VXWM_TAB_HEIGHT;
  xcb_configure_window(sn.conn, win, masks, vals);

  // raise tab window and redraw client tabs
  win_stack(win, Top);
  win_focus(win);
  cln_draw_tabs(fc);
  LOGI("focusing tab %d (%d/%d)\n", win, fc->ft + 1, fc->nt)
}

void bn_focus_page(const arg_t *arg)
{
  client_t *c;

  if (fm->fp == arg->i)
    return;
  
  // lose selection when switching pages
  for (c = next_inpage(fm->cln); c; c = next_inpage(c->next))
    c->sel = 0;
  nsel = 0;

  LOGI("focus page %s\n", pages[arg->i].sym)
  fm->fp = arg->i;
  cln_show_hide(fm);
  cln_set_focus(NULL);
  mon_arrange(fm);
  // TODO: cache the last focused client before switching pages
  cln_set_focus(next_inpage(fm->cln));
  mon_draw_bar(fm);
}

void bn_toggle_tag(const arg_t *arg)
{
  if (fc)
    cln_set_tag(fc, arg->u32, true);
}

void bn_set_tag(const arg_t *arg)
{
  if (fc)
    cln_set_tag(fc, arg->u32, false);
}

void bn_set_param(const arg_t *arg)
{
  int *v = (int *)arg->v;
  
  pages[fm->fp].par[v[0]] += v[1];
  mon_arrange(fm);
  mon_draw_bar(fm);
}

void bn_set_layout(const arg_t *arg)
{
  pages[fm->fp].lt = arg->lt;
  mon_arrange(fm);
  mon_draw_bar(fm);
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

  snprintf(m->lt_status, VXWM_LT_STATUS_BUF, "[%d COL]", arg->par[0]);

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

  snprintf(m->lt_status, VXWM_LT_STATUS_BUF, "[%d/%d STK]", arg->par[0], rn);

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
  scan();
  run();
  cleanup();
  return EXIT_SUCCESS;
}

// vim: ts=2:sw=2:et
