#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

static Display *dpy;

struct window {
  Window da;
  struct window *next;
  cairo_surface_t *surface;
  Pixmap buffer;
  event_handler handle_event;
  void *data;
};

static struct window *all_windows;

Window window_handle(struct window *w)
{
  return w->da;
}

cairo_surface_t *window_surface(struct window *w)
{
  return w->surface;
}

void window_submit_buffer(struct window *w, int width, int height)
{
  if (!w->buffer) return;
  XGCValues xgcv;
  GC gc = XCreateGC(dpy, w->buffer, 0, &xgcv);
  XCopyArea(dpy, w->buffer, w->da, gc, 0, 0, width, height, 0, 0);
  XFlush(dpy);
  XFreeGC(dpy, gc);
}

struct window *window_new(int x, int y, int w, int h, int double_buffer, long event_mask, event_handler evth, void *data)
{
  struct window *win = malloc(sizeof(struct window));
  Window da = XCreateWindow(dpy, DefaultRootWindow(dpy), x, y, w, h, 0, CopyFromParent,
                            InputOutput, CopyFromParent, 0, NULL);
  XSizeHints *size_hints = XAllocSizeHints();
  size_hints->flags = USPosition | USSize;
  size_hints->x = x;
  size_hints->y = y;
  size_hints->width = w;
  size_hints->height = h;
  size_hints->win_gravity = 0;

  XSetWMNormalHints(dpy, da, size_hints);
  XFree(size_hints);

  XSelectInput(dpy, da, event_mask);
  win->da = da;
  win->next = all_windows;
  all_windows = win;
  win->handle_event = evth;
  win->data = data;
  if (double_buffer) {
    win->buffer = XCreatePixmap(dpy, da, w, h, DefaultDepth(dpy, DefaultScreen(dpy)));
    XGCValues xgcv;
    GC gc = XCreateGC(dpy, win->buffer, 0, &xgcv);
    XSetForeground(dpy, gc, -1);
    XFillRectangle(dpy, win->buffer, gc, 0, 0, w, h);
    XFreeGC(dpy, gc);
  } else {
    win->buffer = 0;
  }
  win->surface =
    cairo_xlib_surface_create(
                              dpy, win->buffer ? win->buffer : da,
                              DefaultVisual(dpy, DefaultScreen(dpy)),
                              w, h);

  if (cairo_xlib_surface_get_xrender_format(win->surface) == NULL) {
    fprintf(stderr, "XRender is not available!\n");
  }

  return win;
}

void window_set_redirect_override(struct window *w)
{
  XSetWindowAttributes xswa;
  xswa.override_redirect = True;
  XChangeWindowAttributes(dpy, w->da, CWOverrideRedirect, &xswa);
}

void window_set_event_mask(struct window *w, long event_mask)
{
  XSelectInput(dpy, w->da, event_mask);
}

void window_queue_expose(struct window *w)
{
  XEvent evt;
  evt.xexpose.window = w->da;
  evt.type = Expose;
  XSendEvent(dpy, w->da, False, ExposureMask, &evt);
}

void window_destroy(struct window *w)
{
  if (w->buffer) {
    XFreePixmap(dpy, w->buffer);
    cairo_surface_destroy(w->surface);
  }
  XDestroyWindow(dpy, w->da);
}

#define MWM_HINTS_FUNCTIONS (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE (1L << 2)
#define MWM_HINTS_STATUS (1L << 3)

#define MWM_FUNC_ALL (1L << 0)
#define MWM_FUNC_RESIZE (1L << 1)
#define MWM_FUNC_MOVE (1L << 2)
#define MWM_FUNC_MINIMIZE (1L << 3)
#define MWM_FUNC_MAXIMIZE (1L << 4)
#define MWM_FUNC_CLOSE (1L << 5)

#define MWM_DECOR_ALL (1L << 0)
#define MWM_DECOR_BORDER (1L << 1)
#define MWM_DECOR_RESIZEH (1L << 2)
#define MWM_DECOR_TITLE (1L << 3)
#define MWM_DECOR_MENU (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

typedef struct {
  long flags;
  long functions;
  long decorations;
  long input_mode;
  long status;
} MotifWmHints;

void window_disable_decorator(struct window *w)
{
  Atom hints_atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
  MotifWmHints mwm_hints;
  memset(&mwm_hints, 0, sizeof(MotifWmHints));
  mwm_hints.flags = MWM_HINTS_DECORATIONS;
  mwm_hints.decorations = 0;
  XChangeProperty(dpy, w->da, hints_atom, hints_atom, 32, PropModeReplace,
                  (unsigned char *) &mwm_hints, sizeof(MotifWmHints) / 4);

}

static void window_modify_wm_state_hint(struct window *w, int op, Atom atom)
{
  XEvent event;
  event.xclient.type = ClientMessage;
  event.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
  event.xclient.window = w->da;
  event.xclient.format = 32;
  memset(event.xclient.data.l, 0, 4);
  event.xclient.data.l[0] = op;
  event.xclient.data.l[1] = atom;
  XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

void window_set_sticky(struct window *w)
{
  window_modify_wm_state_hint(w, 1, XInternAtom(dpy, "_NET_WM_STATE_STICKY", False));
}

void window_set_skip_pager(struct window *w)
{
  window_modify_wm_state_hint(w, 1, XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False));
}

void window_set_skip_taskbar(struct window *w)
{
  window_modify_wm_state_hint(w, 1, XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False));
}

void window_set_timestamp(struct window *w, unsigned int ts)
{
  XChangeProperty(dpy, w->da, XInternAtom(dpy, "_NET_WM_USER_TIME", False), XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *) &ts, 1);
}

void window_show(struct window *w)
{
  XMapWindow(dpy, w->da);
}

void window_hide(struct window *w)
{
  XUnmapWindow(dpy, w->da);
}

void window_move(struct window *w, int x, int y)
{
  XMoveWindow(dpy, w->da, x, y);
}

void window_get_position(struct window *w, int *x, int *y)
{
  Window fchild;
  XTranslateCoordinates(dpy, w->da, DefaultRootWindow(dpy), 0, 0, x, y, &fchild);
}

void x11_event_loop(event_handler root_handler, void *data)
{
  while (1) {
    XEvent e;

    XNextEvent(dpy, &e);
    Window target = e.xany.window;

    if (target == DefaultRootWindow(dpy))
      root_handler(NULL, &e, data);
    for (struct window *w = all_windows; w != NULL; w = w->next) {
      if (w->da != target) continue;
      if (w->handle_event)
        w->handle_event(w, &e, w->data);
      break;
    }
  }
}

void x11_init()
{
  if ((dpy = XOpenDisplay(NULL)) == NULL) {
    fputs("Cannot open display", stderr);
    exit(-1);
  }
}

void x11_exit(int status)
{
  XCloseDisplay(dpy);
  exit(status);
}

void x11_bind_key(int keycode, unsigned int modifiers)
{
  Window root = DefaultRootWindow(dpy);
  XGrabKey(dpy, keycode, modifiers, root,
           False, GrabModeAsync, GrabModeAsync);
  XSelectInput(dpy, root, KeyPressMask | KeyReleaseMask);
}

void x11_unbind_key(int keycode, unsigned modifiers)
{
  XUngrabKey(dpy, keycode, modifiers, DefaultRootWindow(dpy));
}

Display *x11_display()
{
  return dpy;
}
