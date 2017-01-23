#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>

#include "window.h"

#include <cairo.h>
#include <pango/pangocairo.h>

#define BORDER_LEN 2
#define X_MARGIN 24
#define Y_MARGIN 10
#define SPC 8
#define ITEM_SIZE 66
#define ITEM_PADDING 1
#define TEXT_Y_MARGIN 4

#define X_OFFSET (X_MARGIN - SPC)

#define FONT "Trebuchet MS Bold 9"

static void change_window_shape(struct window *win, int w, int h, int r)
{
	Display *dpy = x11_display();
	Window window = window_handle(win);
	Pixmap pmap = XCreatePixmap(dpy, window, w, h, 1);
	XGCValues shape_xgcv;
	GC shape_gc = XCreateGC(dpy, pmap, 0, &shape_xgcv);

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

	XFreeGC(dpy, shape_gc);
	XFreePixmap(dpy, pmap);
}

struct switcher_item {
	int id; // non-zero
	int name_len;
	char *name;
	int pixbuf_len;
	void *pixbuf;
	cairo_surface_t *image_surface;

	struct switcher_item *next;
};

struct switcher {
	struct window *win;
	struct switcher_item *selected;
	struct switcher_item *items;
	FILE *provider_process_stdin;
	FILE *provider_process_stdout;
	int nr_items;
	int x, y, w, h;
	int auto_default;

	int trigger_key_code, release_key_code;
	int modifier_mask;
};

static pid_t provider_pid;

static void kill_provider(int sig)
{
	fprintf(stderr, "exiting...\n");
	kill(provider_pid, SIGTERM);
	kill(provider_pid, SIGKILL);
	exit(0);
}

void switcher_run_provider(struct switcher *sw, const char *path, char *const argv[])
{
	int in_pipe[2], out_pipe[2];
	if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0) {
		abort();
	}

	provider_pid = fork();
	if (provider_pid < 0) {
		fprintf(stderr, "Cannot run provider %s\n", path);
		abort();
	}

	if (provider_pid == 0) {
		close(0);
		close(1);
		if (dup2(in_pipe[0], 0) < 0 || dup2(out_pipe[1], 1) < 0) {
			fprintf(stderr, "Cannot dup fd\n");
			exit(-1);
		}
		execv(path, argv);
	} else {
		signal(SIGINT, kill_provider);
		sw->provider_process_stdin = fdopen(in_pipe[1], "w");
		sw->provider_process_stdout = fdopen(out_pipe[0], "r");
	}
}

struct read_closure {
	unsigned char *ptr;
	int offset;
};

static cairo_status_t
cairo_read_from_ptr(void *closure, unsigned char *data, unsigned int length)
{
	struct read_closure *r = closure;
	memcpy(data, r->ptr + r->offset, length);
	r->offset += length;
	return CAIRO_STATUS_SUCCESS;
}

void switcher_read(struct switcher *sw)
{
	fprintf(sw->provider_process_stdin, "list\n");
	fflush(sw->provider_process_stdin);

	FILE *pipe = sw->provider_process_stdout;
	struct switcher_item **parent = &sw->items;
	sw->nr_items = 0;

	while (1) {
		int id, name_len, pixbuf_len;
		if (fread(&id, sizeof(int), 1, pipe) == 0) abort();

		if (id == 0) break;
		if (fread(&name_len, sizeof(int), 1, pipe) == 0) abort();
		if (fread(&pixbuf_len, sizeof(int), 1, pipe) == 0) abort();
		struct switcher_item *item =
			malloc(sizeof(struct switcher_item) + name_len + 1 + pixbuf_len);
		item->next = NULL;
		item->id = id;
		item->name_len = name_len;
		item->pixbuf_len = pixbuf_len;
		item->name = ((char *) item) + sizeof(struct switcher_item);
		item->pixbuf = item->name + name_len + 1;
		if (fread(item->name, name_len, 1, pipe) == 0) abort();
		item->name[name_len] = 0;
		if (fread(item->pixbuf, pixbuf_len, 1, pipe) == 0) abort();

		*parent = item;
		parent = &(item->next);

		struct read_closure closure = {item->pixbuf, 0};
		size_t sz = ITEM_SIZE - 2 * ITEM_PADDING;
		cairo_surface_t *surface =
			cairo_image_surface_create_from_png_stream(
				cairo_read_from_ptr, &closure);
		double img_w = cairo_image_surface_get_width(surface);
		double img_h = cairo_image_surface_get_height(surface);

		cairo_surface_t *scaled_surface =
			cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR_ALPHA, sz, sz);
		cairo_t *scaled_surface_cr = cairo_create(scaled_surface);
		cairo_scale(scaled_surface_cr, sz / img_w, sz / img_h);
		cairo_set_source_surface(scaled_surface_cr, surface, 0, 0);
		cairo_paint(scaled_surface_cr);
		cairo_destroy(scaled_surface_cr);
		cairo_surface_destroy(surface);

		item->image_surface = scaled_surface;
		sw->nr_items++;
	}

	if (sw->auto_default)
		sw->selected = sw->items;
}

void switcher_clear(struct switcher *sw)
{
	struct switcher_item *next;
	for (struct switcher_item *item = sw->items;
	     item != NULL; item = next) {
		next = item->next;
		free(item);
	}
	sw->items = NULL;
	sw->nr_items = 0;
}

static void switcher_done(struct switcher *sw)
{
	if (sw->selected) {
		fprintf(sw->provider_process_stdin,"select\n%d\n", sw->selected->id);
		fflush(sw->provider_process_stdin);
	}
}

static void switcher_calculate_position(struct switcher *sw)
{
	Display *dpy = x11_display();
	// Window  root = DefaultRootWindow(dpy);
	Window focus, fchild;
	int revert_to = 0;
	int fx, fy;
	XGetInputFocus(dpy, &focus, &revert_to);
	XTranslateCoordinates(dpy, focus, DefaultRootWindow(dpy), 0, 0, &fx, &fy, &fchild);

	XRRScreenResources *res = XRRGetScreenResources(dpy, focus);
	int done = 0;
	for (int i = 0; !done && i < res->ncrtc; i++) {
		XRRCrtcInfo *info = XRRGetCrtcInfo(dpy, res, res->crtcs[i]);
		fprintf(stderr, "crtc %dx%d %d,%d\n", info->width, info->height, info->x, info->y);
		if (fx >= info->x && fx < info->x + info->width
		    && fy >= info->y && fy < info->y + info->height) {
			sw->x = info->x + (info->width - sw->w) / 2;
			sw->y = info->y + (info->height - sw->h) / 2;
			done = 1;
		}
		XRRFreeCrtcInfo(info);
	}
	XRRFreeScreenResources(res);
}

static void round_rect(cairo_t *cr, int x, int y, int w, int h, double r)
{
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + r, y + r, r, M_PI, 1.5 * M_PI);
	cairo_arc(cr, x + w - r, y + r, r, 1.5 * M_PI, 2 * M_PI);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, 0.5 * M_PI);
	cairo_arc(cr, x + r, y + h - r, r, 0.5 * M_PI, M_PI);
	cairo_close_path(cr);
}

// icon switcher

static void icon_switcher_paint(struct switcher *sw, cairo_t *cr)
{
	round_rect(cr, BORDER_LEN / 2, BORDER_LEN / 2, sw->w - BORDER_LEN, sw->h - BORDER_LEN , 16 - BORDER_LEN / 2);
	cairo_set_source_rgb(cr, 1., 1., 1.);
	cairo_stroke(cr);

	cairo_pattern_t *pattern =
		cairo_pattern_create_linear(0, 0, 0, sw->h - 2 * BORDER_LEN);
	cairo_pattern_add_color_stop_rgb(pattern, 0, 0x2E / 256., 0x70 / 256., 0xD6 / 256.);
	cairo_pattern_add_color_stop_rgb(pattern, 1, 0x05 / 256., 0x3B / 256., 0x8F / 256.);
	cairo_set_source(cr, pattern);
	round_rect(cr, BORDER_LEN, BORDER_LEN, sw->w - BORDER_LEN * 2, sw->h - BORDER_LEN * 2, 16 - BORDER_LEN);
	cairo_fill(cr);
	cairo_pattern_destroy(pattern);

	int pos = 0;
	for (struct switcher_item *item = sw->items; item != NULL; item = item->next, pos++) {
		if (item == sw->selected) {
			round_rect(cr, X_OFFSET + pos * (SPC + ITEM_SIZE), Y_MARGIN,
				   ITEM_SIZE, ITEM_SIZE, 16);
			cairo_set_source_rgb(cr, 0x17 / 256., 0x3b / 256., 0x73 / 256.);
			cairo_fill(cr);
			round_rect(cr, X_OFFSET + pos * (SPC + ITEM_SIZE), Y_MARGIN,
				   ITEM_SIZE, ITEM_SIZE, 16);
			cairo_set_source_rgb(cr, 1., 1., 1.);
			cairo_set_line_width(cr, 2);
			cairo_stroke(cr);

			PangoLayout *layout = pango_cairo_create_layout(cr);
			PangoFontDescription *desc = pango_font_description_from_string(FONT);
			pango_layout_set_text(layout, item->name, item->name_len);
			pango_layout_set_font_description(layout, desc);
			pango_font_description_free(desc);

			int text_width, text_height;
			pango_layout_get_size(layout, &text_width, &text_height);
			int xpos = X_OFFSET + pos * (SPC + ITEM_SIZE) + ITEM_SIZE / 2;
			xpos -= text_width / PANGO_SCALE / 2;

			if (xpos + text_width / PANGO_SCALE > sw->w - X_OFFSET)
				xpos = sw->w - X_MARGIN - text_width / PANGO_SCALE;
			if (xpos < X_OFFSET)
				xpos = X_OFFSET;

			cairo_move_to(cr, xpos, Y_MARGIN + ITEM_SIZE + TEXT_Y_MARGIN);
			pango_cairo_show_layout(cr, layout);
			g_object_unref(layout);
		}
		// draw the image
		double x_off = X_OFFSET + pos * (SPC + ITEM_SIZE) + ITEM_PADDING;
		double y_off = Y_MARGIN + ITEM_PADDING;

		cairo_set_source_surface(cr, item->image_surface, x_off, y_off);
		cairo_paint(cr);
	}
}

static void icon_switcher_next(struct switcher *sw)
{
	if (sw->selected == NULL) sw->selected = sw->items;
	else sw->selected = sw->selected->next;
	if (sw->auto_default && sw->selected == NULL) sw->selected = sw->items;
}

static void icon_switcher_calculate_size(struct switcher *sw)
{
	sw->h = ITEM_SIZE + 2 * (Y_MARGIN + TEXT_Y_MARGIN) + 12;
	sw->w = sw->nr_items * ITEM_SIZE + (sw->nr_items - 2) * SPC + 2 * X_MARGIN;
	switcher_calculate_position(sw);
}

static void icon_switcher_done(struct switcher *sw)
{
	if (sw->items == NULL)
		return;
	switcher_done(sw);
	usleep(50000);
	window_hide(sw->win);
	window_destroy(sw->win);
	switcher_clear(sw);
}

static void icon_switcher_event_handler(struct window *w, XEvent *event, void *data)
{
	struct switcher *sw = data;
	int fx, fy;
	cairo_t *cr;
	switch (event->type) {
	case Expose:
		if (sw->items == NULL)
			break;
		// fprintf(stderr, "Graphic: expose\n");

		change_window_shape(sw->win, sw->w, sw->h, 16);

		cr = cairo_create(window_surface(sw->win));
		icon_switcher_paint(sw, cr);
		window_submit_buffer(w, sw->w, sw->h);
		cairo_destroy(cr);
		break;
	case NoExpose:
		break;
	default:
		printf("unhandled event %d\n", event->type);
	}
}

static void icon_switcher_switch_or_show(struct switcher *sw, unsigned int timestamp)
{
	if (sw->items == NULL) {
		switcher_read(sw);
		if (sw->items) {
			icon_switcher_calculate_size(sw);
			sw->win = window_new(sw->x, sw->y, sw->w, sw->h,
					     1,
					     ExposureMask,
					     icon_switcher_event_handler, sw);
			window_set_redirect_override(sw->win);
			window_disable_decorator(sw->win);
			window_set_sticky(sw->win);
			window_set_skip_pager(sw->win);
			window_set_skip_taskbar(sw->win);
			window_set_timestamp(sw->win, timestamp);
			window_show(sw->win);
			// window_move(sw->win, sw->x, sw->y);
		}
	} else {
		icon_switcher_next(sw);
		window_queue_expose(sw->win);
	}
}


static struct switcher *gsw;

static void icon_switcher_trigger(struct window *w, XEvent *event, void *data)
{
	struct switcher *sw = data;
	switch (event->type) {
	case KeyPress:
		fprintf(stderr, "Global Press %d\n", event->xkey.keycode);
		if (event->xkey.keycode == 23) { // tab
			icon_switcher_switch_or_show(sw, event->xkey.time);
			return;
		}
		break;
	case KeyRelease:
		fprintf(stderr, "Global Release %d\n", event->xkey.keycode);
		if (event->xkey.keycode == 23) {
			return;
		}
		if (event->xkey.keycode == 64 && sw->items != NULL) {
			icon_switcher_done(sw);
		}
		break;
	}

	// forward whatever event
	fprintf(stderr, "forwarding trigger event\n");
	Window focus_window;
	int revert_to;
	XGetInputFocus(x11_display(), &focus_window, &revert_to);
	XKeyEvent evt = event->xkey;
	evt.window = focus_window;
	XSendEvent(x11_display(), focus_window, True, KeyPressMask, (XEvent *) &evt);
}

static void show_usage()
{
	exit(-1);
}

int main(int argc, char *argv[])
{
	gsw = malloc(sizeof(struct switcher));
	memset(gsw, 0, sizeof(struct switcher));

	gsw->auto_default = 1;
	gsw->trigger_key_code = 23; // Tab
	gsw->release_key_code = 64; // Alt
	gsw->modifier_mask = 8; // Alt

	int opt;
	int key_code;
	while ((opt = getopt(argc, argv, "t:r:m:"))) {
		switch (opt) {
		case 't':
			key_code = atoi(optarg);
			if (key_code != 0) gsw->trigger_key_code = key_code;
			break;
		case 'r':
			key_code = atoi(optarg);
			if (key_code != 0) gsw->release_key_code = key_code;
			break;
		case 'm':
			key_code = atoi(optarg);
			if (key_code != 0) gsw->modifier_mask = key_code;
			break;
		default:
			goto start;
		}
	}
start:
	x11_init();
	x11_bind_key(gsw->trigger_key_code, gsw->modifier_mask);
	x11_bind_key(gsw->release_key_code, 0);

	if (argc < 2) {
		show_usage();
	}

	char *pipe_argv[argc];
	memcpy(pipe_argv, argv + optind, sizeof(char *) * (argc - optind));
	pipe_argv[argc - 1] = NULL;

	switcher_run_provider(gsw, pipe_argv[0], pipe_argv);

	x11_event_loop(icon_switcher_trigger, gsw);
	return 0;
}
