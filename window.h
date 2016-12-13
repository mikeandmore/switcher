#ifndef WINDOW_H
#define WINDOW_H
#include <X11/Xlib.h>
#include <cairo/cairo.h>

struct window;
typedef void (*event_handler)(struct window *, XEvent *, void *data);

struct window *window_new(int x, int y, int w, int h,
			  long event_mask, event_handler evth, void *data);
void window_destroy(struct window *w);

void window_disable_decorator(struct window *w);
void window_show(struct window *w);
void window_hide(struct window *w);
Window window_handle(struct window *w);
cairo_surface_t *window_surface(struct window *w);

void x11_event_loop(event_handler root_handler, void *data);
void x11_bind_key(int keycode, unsigned int modifiers);
void x11_init();
void x11_exit();
Display *x11_display();

#endif /* WINDOW_H */
