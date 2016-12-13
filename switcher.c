#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/extensions/shape.h>
#include "window.h"

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

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
	struct switcher_item *selected;
	struct switcher_item *items;
	int nr_items;
	int w, h;
};

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

void switcher_read(struct switcher *sw, FILE *pipe)
{
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
		item->image_surface =
			cairo_image_surface_create_from_png_stream(
				cairo_read_from_ptr, &closure);
		sw->nr_items++;
	}
}

void switcher_destroy(struct switcher *sw)
{
	struct switcher_item *next;
	for (struct switcher_item *item = sw->items;
	     item != NULL; item = next) {
		next = item->next;
		free(item);
	}
	free(sw);
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

#define BORDER_LEN 2
#define X_MARGIN 24
#define Y_MARGIN 10
#define SPC 8
#define ITEM_SIZE 65
#define ITEM_PADDING 1
#define TEXT_Y_MARGIN 4

#define X_OFFSET (X_MARGIN - SPC)

#define FONT "Trebuchet MS Bold 9"

static void icon_switcher_paint(struct switcher *sw, cairo_t *cr)
{
	cairo_set_source_rgb(cr, 1., 1., 1.);
	cairo_paint(cr);

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
		double img_w = cairo_image_surface_get_width(item->image_surface);
		double img_h = cairo_image_surface_get_height(item->image_surface);
		cairo_translate(cr,
				X_OFFSET + pos * (SPC + ITEM_SIZE) + ITEM_PADDING,
				Y_MARGIN + ITEM_PADDING);
		cairo_scale(cr,
			    1. * (ITEM_SIZE - 2 * ITEM_PADDING) / img_w,
			    1. * (ITEM_SIZE - 2 * ITEM_PADDING) / img_h);
		cairo_set_source_surface(cr, item->image_surface, 0, 0);
		cairo_paint(cr);
		cairo_identity_matrix(cr);
	}
}

static void icon_switcher_next(struct switcher *sw)
{
	sw->selected = sw->selected->next;
	if (sw->selected == NULL) sw->selected = sw->items;
}

static void done(struct switcher *sw)
{
	printf("%d\n", sw->selected ? sw->selected->id : 0);
	x11_exit(0);
}

static void icon_switcher_event_handler(struct window *w, XEvent *event, void *data)
{
	struct switcher *sw = data;
	cairo_t *cr;
	switch (event->type) {
	case Expose:
		cr = cairo_create(window_surface(w));
		icon_switcher_paint(sw, cr);
		cairo_destroy(cr);
		break;
	case KeyPress:
		fprintf(stderr, "Press keycode %d\n", event->xkey.keycode);
		if (event->xkey.keycode == 23) { // tab
			icon_switcher_next(sw);
			XEvent evt;
			evt.xexpose.window = window_handle(w);
			evt.type = Expose;
			XSendEvent(x11_display(), window_handle(w), False,
				   ExposureMask, &evt);
		} else if (event->xkey.keycode == 36) {
			done(sw);
		}
		break;
	case KeyRelease:
		fprintf(stderr, "Press keycode %d\n", event->xkey.keycode);
		if (event->xkey.keycode == 64) {
			done(sw);
		}
		break;
	default:
		printf("unhandled event %d\n", event->type);
	}
}

static void icon_switcher_calculate_size(struct switcher *sw)
{
	sw->h = ITEM_SIZE + 2 * (Y_MARGIN + TEXT_Y_MARGIN) + 12;
	sw->w = sw->nr_items * ITEM_SIZE + (sw->nr_items - 2) * SPC + 2 * X_MARGIN;
}

static void icon_switcher_trigger(struct window *w, XEvent *event, void *data)
{
	struct switcher *sw = data;
	fprintf(stderr, "global event type %d\n", event->type);
}

static switcher *gsw;

int main(int argc, char *argv[])
{
	x11_init();
	x11_bind_key(23, 8); // Alt Tab

	gsw = malloc(sizeof(struct switcher));
	memset(gsw, 0, sizeof(struct switcher));
/*
	switcher_read(sw);

	// set the default to first
	sw->selected = sw->items;

	icon_switcher_calculate_size(sw);
	struct window *main_w =
		window_new(0, 0, sw->w, sw->h,
			   KeyPressMask | KeyReleaseMask | ExposureMask,
			   icon_switcher_event_handler, sw);

	window_disable_decorator(main_w);
	// window_show(main_w);
	// change_window_shape(main_w, sw->w, sw->h, 16);
	*/
	x11_event_loop(icon_switcher_trigger, sw);
	return 0;
}
