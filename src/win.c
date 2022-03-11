#include <string.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>
#include "win.h"
#include "util.h"

static xcb_get_property_reply_t *win_get_prop(xcb_window_t, xcb_atom_t, xcb_atom_t, int *);

// caller is responsible for freeing the return value
xcb_get_property_reply_t *win_get_prop(xcb_window_t win, xcb_atom_t prop, xcb_atom_t type, int *len)
{
  xcb_get_property_cookie_t pc;
  xcb_get_property_reply_t *pr;

  pc = xcb_get_property(sn.conn, 0, win, prop, type, 0, UINT32_MAX);
  pr = xcb_get_property_reply(sn.conn, pc, NULL);
  // TODO: check for errors in reply
  if (len)
    *len = xcb_get_property_value_length(pr);
  return pr;
}

void win_get_geometry(xcb_window_t win, int *x, int *y, int *w, int *h, int *bw)
{
  xcb_get_geometry_cookie_t gc;
  xcb_get_geometry_reply_t *gr;

  gc = xcb_get_geometry(sn.conn, win);
  gr = xcb_get_geometry_reply(sn.conn, gc, NULL);
  if (gr) {
    if (x)
      *x = (int)gr->x;
    if (y)
      *y = (int)gr->y;
    if (w)
      *w = (int)gr->width;
    if (h)
      *h = (int)gr->height;
    if (bw)
      *bw = (int)gr->border_width;
    xfree(gr);
  }
}

xcb_atom_t win_get_atom_prop(xcb_window_t win, xcb_atom_t prop)
{
  xcb_get_property_reply_t *pr;
  xcb_atom_t *atom;

  pr = win_get_prop(win, prop, XCB_ATOM_ATOM, NULL);
  atom = (xcb_atom_t *)xcb_get_property_value(pr);
  xfree(pr);
  if (!atom)
    return XCB_ATOM_NONE;
  return *atom;
}

bool win_get_text_prop(xcb_window_t win, xcb_atom_t prop, char *buf, uint32_t buf_len)
{
  xcb_get_property_cookie_t pc;
  xcb_icccm_get_text_property_reply_t tpr;

  if (!buf || buf_len == 0)
    return false;
  pc = xcb_icccm_get_text_property(sn.conn, win, prop);
  if (!xcb_icccm_get_text_property_reply(sn.conn, pc, &tpr, NULL)) {
    LOGW("failed to retrieve %d text property for %d\n", prop, win)
  }
  if (tpr.name_len == 0 || tpr.name_len >= buf_len) {
    xcb_icccm_get_text_property_reply_wipe(&tpr);
    return false;
  }
  strncpy(buf, tpr.name, tpr.name_len);
  buf[tpr.name_len] = '\0';
  xcb_icccm_get_text_property_reply_wipe(&tpr);
  return true;
}

void win_stack(xcb_window_t win, pos_t p)
{
  uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
  uint32_t val = p == Top ? XCB_STACK_MODE_ABOVE : XCB_STACK_MODE_BELOW;
  xcb_configure_window(sn.conn, win, mask, &val);
}

/// Sets the input focus window.
/// If the window supports WM_TAKE_FOCUS protocol, send a client message to it.
void win_focus(xcb_window_t win)
{
  if (win != sn.root && win_has_proto(win, sn.wm_atom[WmTakeFocus]))
    win_send_proto(win, sn.wm_atom[WmTakeFocus]);
  xcb_set_input_focus(sn.conn, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
}

/// Sets the WM_STATE property of a window.
void win_set_state(xcb_window_t win, uint32_t state)
{
  uint32_t data[] = { state, XCB_NONE };
  xcb_change_property(sn.conn, XCB_PROP_MODE_REPLACE, win,
                      sn.wm_atom[WmState], sn.wm_atom[WmState], 32, 2, data);
}

/// Queries if a window has a certain WM_PROTOCOL property.
bool win_has_proto(xcb_window_t win, xcb_atom_t proto)
{
  xcb_get_property_cookie_t pc;
  xcb_icccm_get_wm_protocols_reply_t wmpr;
  int i, n;

  pc = xcb_icccm_get_wm_protocols(sn.conn, win, sn.wm_atom[WmProtocols]);
  if (!xcb_icccm_get_wm_protocols_reply(sn.conn, pc, &wmpr, NULL)) {
    LOGW("failed to retrieve wm protocols for %d\n", win)
    return false;
  }
  for (i = 0, n = wmpr.atoms_len; i < n && wmpr.atoms[i] != proto; i++) ;
  xcb_icccm_get_wm_protocols_reply_wipe(&wmpr);
  return i != n;
}

/// Sends a WM_PROTOCOL client message to a window.
void win_send_proto(xcb_window_t win, xcb_atom_t proto)
{
  xcb_client_message_event_t msg;

  msg.response_type = XCB_CLIENT_MESSAGE;
  msg.format = 32;
  msg.window = win;
  msg.type = sn.wm_atom[WmProtocols];
  msg.data.data32[0] = proto;
  msg.data.data32[1] = XCB_TIME_CURRENT_TIME;
  xcb_send_event(sn.conn, false, win, XCB_EVENT_MASK_NO_EVENT, (const char *)&msg);
}

/// If the window supports WM_DELETE_WINDOW protocol, send a client message to it.
/// Otherwise kill it directly from our side.
void win_kill(xcb_window_t win)
{
  if (win_has_proto(win, sn.wm_atom[WmDeleteWindow]))
    win_send_proto(win, sn.wm_atom[WmDeleteWindow]);
  else
    xcb_kill_client(sn.conn, win);
}

// vim: ts=2:sw=2:et
