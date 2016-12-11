#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

struct window {
	Display *dpy;
	Window da;
	int x, y, w, h, r;
};

static void change_window_shape(Display *dpy, Window window)
{
	Pixmap pmap = XCreatePixmap(dpy, window, width, height, 1);
	XGCValues shape_xgcv;
	GC shape_gc = XCreateGC(dpy, pmap, 0, &shape_xgcv);
	int w = width, h = height, r = window_radius;
	XSetForeground(dpy, shape_gc, 0);
	XFillRectangle(dpy, pmap, shape_gc, 0, 0, w, h);
	XSetForeground(dpy, shape_gc, 1);

	// draw the shape_gc
 	XFillArc(dpy, pmap, shape_gc, 0,         0,         2 * r, 2 * r, 0, 360 * 64);
	XFillArc(dpy, pmap, shape_gc, w - 2 * r, 0,		2 * r, 2 * r, 0, 360 * 64);
	XFillArc(dpy, pmap, shape_gc, w - 2 * r, h - 2 * r, 2 * r, 2 * r, 0, 360 * 64);
	XFillArc(dpy, pmap, shape_gc, 0,	     h - 2 * r, 2 * r, 2 * r, 0, 360 * 64);

	XFillRectangle(dpy, pmap, shape_gc, r, 0, w - 2 * r, r);
	XFillRectangle(dpy, pmap, shape_gc, r, h - r, w - 2 * r, r);
	XFillRectangle(dpy, pmap, shape_gc, 0, r, w, h - 2 * r);

	XShapeCombineMask(dpy, window, ShapeBounding, 0, 0, pmap, ShapeSet);

	XFreegC(shape_gc);
	XFreePixmap(pmap);
}

void window_init(struct window *window, int x, int y, int w, int h, int r)
{
	Display *dpy;
	if ((dpy = XOpenDisplay(NULL)) == NULL) {
		fputs("Cannot open display", stderr);
		exit(-1);
	}

	Window da = XCreateSmipleWindow(dpy, DefaultRootWindow(dpy),
					x, y, w, h, 0, 0, 0);
	XSelectInput(dpy, da, KeyReleaseMask | ExposureMask);
	XMapWindow(dpy, da);
	change_window_shape(dpy, da);

	window->x = x;
	window->y = y;
	window->w = w;
	window->h = h;
	window->r = r;
	window->dpy = dpy;
	window->da = da;
}

void x11_event_loop(Display *dpy)
{
	while (1) {
		XEvent e;
		XNextEvent(dpy, &e);
		switch (e.type) {

		}
	}
}
