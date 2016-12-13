#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cairo/cairo-xlib.h>

static Display *dpy;

struct window {
	Window da;
	struct window *next;
	cairo_surface_t *surface;
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

struct window *window_new(int x, int y, int w, int h, long event_mask, event_handler evth, void *data)
{
	struct window *win = malloc(sizeof(struct window));
	Window da = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
					x, y, w, h, 0, 0, 0);
	XSelectInput(dpy, da, event_mask);
	win->da = da;
	win->next = all_windows;
	all_windows = win;
	win->handle_event = evth;
	win->data = data;
	win->surface =
		cairo_xlib_surface_create(
			dpy, win->da,
			DefaultVisual(dpy, DefaultScreen(dpy)),
			w, h);
	return win;
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

void window_show(struct window *w)
{
	XMapWindow(dpy, w->da);
}

void window_hide(struct window *w)
{
	XUnmapWindow(dpy, w->da);
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

Display *x11_display()
{
	return dpy;
}
