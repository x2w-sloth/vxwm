#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "util.h"
#include "vxwm.h"

#ifdef VXWM_DEBUG

void xassert(bool assertion, const char *emsg)
{
  if (assertion == true)
    return;
  die(emsg);
}

void log_event(xcb_generic_event_t *ge)
{
  switch (XCB_EVENT_RESPONSE_TYPE(ge)) {
    case XCB_KEY_PRESS:
      fprintf(VXWM_LOG_FILE, "xcb_key_press: %d\n",
        ((xcb_key_press_event_t *)ge)->event);
      break;
    case XCB_BUTTON_PRESS:
      fprintf(VXWM_LOG_FILE, "xcb_button_press:   %d @ (%d, %d)\n",
        ((xcb_button_press_event_t *)ge)->event,
        ((xcb_button_press_event_t *)ge)->root_x,
        ((xcb_button_press_event_t *)ge)->root_y);
      break;
    case XCB_BUTTON_RELEASE:
      fprintf(VXWM_LOG_FILE, "xcb_button_release: %d @ (%d, %d)\n",
        ((xcb_button_release_event_t *)ge)->event,
        ((xcb_button_release_event_t *)ge)->root_x,
        ((xcb_button_release_event_t *)ge)->root_y);
      break;
    case XCB_MOTION_NOTIFY:
      fprintf(VXWM_LOG_FILE, "xcb_motion_notify: (%d, %d)\n",
        ((xcb_motion_notify_event_t *)ge)->root_x,
        ((xcb_motion_notify_event_t *)ge)->root_y);
      break;
    case XCB_FOCUS_IN:
      fprintf(VXWM_LOG_FILE, "xcb_focus_in:  %d\n",
        ((xcb_focus_in_event_t *)ge)->event);
      break;
    case XCB_EXPOSE:
      break;
    case XCB_DESTROY_NOTIFY:
      fprintf(VXWM_LOG_FILE, "xcb_destroy_notify: %d\n",
        ((xcb_destroy_notify_event_t *)ge)->window);
      break;
    case XCB_UNMAP_NOTIFY:
      fprintf(VXWM_LOG_FILE, "xcb_unmap_notify:   %d\n",
        ((xcb_unmap_notify_event_t *)ge)->window);
      break;
    case XCB_MAP_REQUEST:
      fprintf(VXWM_LOG_FILE, "xcb_map_request:    %d\n",
        ((xcb_map_request_event_t *)ge)->window);
      break;
    case XCB_CONFIGURE_REQUEST:
      fprintf(VXWM_LOG_FILE, "xcb_configure_request: %d\n",
        ((xcb_configure_request_event_t *)ge)->window);
      break;
    case XCB_CLIENT_MESSAGE:
      fprintf(VXWM_LOG_FILE, "xcb_client_message: %d\n",
        ((xcb_client_message_event_t *)ge)->window);
      break;
    default:
      break;
  }
}

#endif // VXWM_DEBUG

void *xmalloc(size_t size)
{
  void *mem = malloc(size);
  if (!mem)
    die("failed to allocate memory");
  return mem;
}

void xfree(void *mem)
{
  if (!mem)
    return;
  free(mem);
}

void die(const char *emsg)
{
  fputs(emsg, VXWM_ERR_FILE);
  fputc('\n', VXWM_ERR_FILE);
  exit(EXIT_FAILURE);
}

// vim: ts=2:sw=2:et
