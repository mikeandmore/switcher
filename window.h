#ifndef WINDOW_H
#define WINDOW_H
#include <X11/Xlib.h>
#include <cairo.h>

struct window;
typedef void (*event_handler)(struct window *, XEvent *, void *data);

struct window *window_new(int x, int y, int w, int h, int double_buffer,
			  long event_mask, event_handler evth, void *data);

void window_set_event_mask(struct window *w, long event_mask);
void window_queue_expose(struct window *w);

void window_destroy(struct window *w);

void window_disable_decorator(struct window *w);
void window_set_redirect_override(struct window *w);
void window_set_sticky(struct window *w);
void window_set_skip_pager(struct window *w);
void window_set_skip_taskbar(struct window *w);
void window_set_timestamp(struct window *w, unsigned int ts);

void window_show(struct window *w);
void window_hide(struct window *w);
void window_move(struct window *w, int x, int y);
void window_get_position(struct window *w, int *x, int *y);

Window window_handle(struct window *w);
cairo_surface_t *window_surface(struct window *w);
void window_submit_buffer(struct window *w, int width, int height);

void x11_event_loop(event_handler root_handler, void *data);
void x11_bind_key(int keycode, unsigned int modifiers);
void x11_unbind_key(int keycode, unsigned modifiers);

void x11_init();
void x11_exit();
Display *x11_display();


#endif /* WINDOW_H */
