/* Eye of Gnome image viewer - image canvas item
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <math.h>
#include "image.h"
#include "image-item.h"
#include "render.h"



/* Check values */
#define DARK_CHECK 0x55
#define LIGHT_CHECK 0xaa



/* Private part of the ImageItem structure */
typedef struct {
	/* Image being displayed */
	Image *image;

	/* User-specified width and height */
	double uwidth, uheight;

	/* Size of the image after zooming */
	int cwidth, cheight;

	/* Whether the image has changed */
	guint need_image_update : 1;

	/* Whether the size has changed */
	guint need_size_update : 1;
} ImageItemPrivate;



/* Object argument IDs */
enum {
	ARG_0,
	ARG_IMAGE,
	ARG_WIDTH,
	ARG_HEIGHT
};

static void image_item_class_init (ImageItemClass *class);
static void image_item_init (ImageItem *ii);
static void image_item_destroy (GtkObject *object);
static void image_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void image_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void image_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void image_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			     int x, int y, int width, int height);
static double image_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
				GnomeCanvasItem **actual_item);
static void image_item_bounds (GnomeCanvasItem *item,
			       double *x1, double *y1, double *x2, double *y2);


static GnomeCanvasItemClass *parent_class;



/**
 * image_item_get_type:
 * @void:
 *
 * Registers the #ImageItem class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #ImageItem class.
 **/
GtkType
image_item_get_type (void)
{
	static GtkType image_item_type = 0;

	if (!image_item_type) {
		static const GtkTypeInfo image_item_info = {
			"ImageItem",
			sizeof (ImageItem),
			sizeof (ImageItemClass),
			(GtkClassInitFunc) image_item_class_init,
			(GtkObjectInitFunc) image_item_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		image_item_type = gtk_type_unique (gnome_canvas_item_get_type (), &image_item_info);
	}

	return image_item_type;
}

/* Class initialization function for the image item */
static void
image_item_class_init (ImageItemClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("ImageItem::image",
				 GTK_TYPE_POINTER, GTK_ARG_READWRITE, ARG_IMAGE);
	gtk_object_add_arg_type ("ImageItem::width",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_WIDTH);
	gtk_object_add_arg_type ("ImageItem::height",
				 GTK_TYPE_DOUBLE, GTK_ARG_READWRITE, ARG_HEIGHT);

	object_class->destroy = image_item_destroy;
	object_class->set_arg = image_item_set_arg;
	object_class->get_arg = image_item_get_arg;

	item_class->update = image_item_update;
	item_class->draw = image_item_draw;
	item_class->point = image_item_point;
	item_class->bounds = image_item_bounds;
}

/* Object initialization function for the image item */
static void
image_item_init (ImageItem *ii)
{
	ImageItemPrivate *priv;

	priv = g_new0 (ImageItemPrivate, 1);
	ii->priv = priv;

	priv->uwidth = priv->uheight = 0.0;
}

/* Destroy handler for the image item */
static void
image_item_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	ImageItem *ii;
	ImageItemPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_IMAGE_ITEM (object));

	item = GNOME_CANVAS_ITEM (object);
	ii = IMAGE_ITEM (object);
	priv = ii->priv;

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	if (priv->image)
		image_unref (priv->image);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* Set_arg handler for the image item */
static void
image_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	ImageItem *ii;
	ImageItemPrivate *priv;
	Image *image;
	double val;

	item = GNOME_CANVAS_ITEM (object);
	ii = IMAGE_ITEM (object);
	priv = ii->priv;

	switch (arg_id) {
	case ARG_IMAGE:
		image = GTK_VALUE_POINTER (*arg);
		if (image != priv->image) {
			if (image)
				image_ref (image);

			if (priv->image)
				image_unref (priv->image);

			priv->image = image;
		}

		priv->need_image_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_WIDTH:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->uwidth = val;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_HEIGHT:
		val = GTK_VALUE_DOUBLE (*arg);
		g_return_if_fail (val >= 0.0);
		priv->uheight = val;
		priv->need_size_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}

/* Get_arg handler for the image item */
static void
image_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ImageItem *ii;
	ImageItemPrivate *priv;

	ii = IMAGE_ITEM (object);
	priv = ii->priv;

	switch (arg_id) {
	case ARG_IMAGE:
		GTK_VALUE_POINTER (*arg) = priv->image;
		break;

	case ARG_WIDTH:
		GTK_VALUE_DOUBLE (*arg) = priv->uwidth;
		break;

	case ARG_HEIGHT:
		GTK_VALUE_DOUBLE (*arg) = priv->uheight;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/* Recomputes the bounding box of an image item */
static void
recompute_bounding_box (ImageItem *ii)
{
	GnomeCanvasItem *item;
	ImageItemPrivate *priv;
	double i2c[6];
	ArtPoint i, c;

	item = GNOME_CANVAS_ITEM (ii);
	priv = ii->priv;

	if (priv->image && priv->image->pixbuf) {
		priv->cwidth = floor (priv->uwidth * item->canvas->pixels_per_unit + 0.5);
		priv->cheight = floor (priv->uheight * item->canvas->pixels_per_unit + 0.5);
	} else
		priv->cwidth = priv->cheight = 0;

	gnome_canvas_item_i2c_affine (item, i2c);

	i.x = 0.0;
	i.y = 0.0;
	art_affine_point (&c, &i, i2c);

	item->x1 = c.x;
	item->y1 = c.y;
	item->x2 = item->x1 + priv->cwidth;
	item->y2 = item->y1 + priv->cheight;
}

/* Update handler for the image item */
static void
image_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	ImageItem *ii;
	ImageItemPrivate *priv;

	ii = IMAGE_ITEM (item);
	priv = ii->priv;

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	if (((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
	     && !(GTK_OBJECT_FLAGS (ii) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)
	    || priv->need_image_update
	    || priv->need_size_update)
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	/* If we need an image update, or if the item changed visibility to
	 * shown, recompute the bounding box.
	 */
	if (priv->need_image_update
	    || priv->need_size_update
	    || ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
		&& (GTK_OBJECT_FLAGS (ii) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)) {
		recompute_bounding_box (ii);
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
		priv->need_image_update = FALSE;
		priv->need_size_update = FALSE;
	}
}

/* Draw handler for the image item */
static void
image_item_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	ImageItem *ii;
	ImageItemPrivate *priv;
	ArtIRect i, d, dest;
	int w, h;
	guchar *buf;
	GdkPixbuf *tmp;

	ii = IMAGE_ITEM (item);
	priv = ii->priv;

	if (!(priv->image && priv->image->pixbuf))
		return;

	i.x0 = item->x1;
	i.y0 = item->y1;
	i.x1 = item->x2;
	i.y1 = item->y2;

	d.x0 = x;
	d.y0 = y;
	d.x1 = x + width;
	d.y1 = y + height;

	art_irect_intersect (&dest, &i, &d);

	if (art_irect_empty (&dest))
		return;

	w = dest.x1 - dest.x0;
	h = dest.y1 - dest.y0;

	tmp = gdk_pixbuf_new (ART_PIX_RGB, FALSE, 8, w, h);
	gdk_pixbuf_composite_color (priv->image->pixbuf, tmp,
				    0, 0, w, h,
				    -(dest.x0 - item->x1), -(dest.y0 - item->y1),
				    item->canvas->pixels_per_unit, item->canvas->pixels_per_unit,
				    ART_FILTER_BILINEAR,
				    255,
				    dest.x0 - item->x1, dest.y0 - item->y1,
				    16,
				    0x00555555, 0x00aaaaaa);
	gdk_draw_rgb_image_dithalign (drawable, GTK_WIDGET (item->canvas)->style->black_gc,
				      dest.x0 - x, dest.y0 - y,
				      w, h,
				      GDK_RGB_DITHER_NORMAL,
				      gdk_pixbuf_get_pixels (tmp),
				      gdk_pixbuf_get_rowstride (tmp),
				      x, y);
	gdk_pixbuf_unref (tmp);

#if 0
	buf = g_new (guchar, w * h * 3);
	render_image (priv->image->pixbuf, buf,
		      w, h, w * 3,
		      priv->uwidth * item->canvas->pixels_per_unit,
		      priv->uheight * item->canvas->pixels_per_unit,
		      dest.x0 - item->x1, dest.y0 - item->y1,
		      priv->image->r_lut, priv->image->g_lut, priv->image->b_lut,
		      DARK_CHECK, LIGHT_CHECK);

	gdk_draw_rgb_image_dithalign (drawable, GTK_WIDGET (item->canvas)->style->black_gc,
				      dest.x0 - x, dest.y0 - y,
				      w, h,
				      GDK_RGB_DITHER_NORMAL,
				      buf, w * 3,
				      x, y);
	g_free (buf);
#endif
}

/* Point handler for the image item */
static double
image_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
		  GnomeCanvasItem **actual_item)
{
	ImageItem *ii;
	ImageItemPrivate *priv;
	double w, h;
	double dx, dy;

	ii = IMAGE_ITEM (item);
	priv = ii->priv;

	*actual_item = item;

	w = priv->cwidth / item->canvas->pixels_per_unit;
	h = priv->cheight / item->canvas->pixels_per_unit;

	if (x < 0.0)
		dx = -x;
	else if (x > w)
		dx = x - w;
	else
		dx = 0.0;

	if (y < 0.0)
		dy = -y;
	else if (y > h)
		dy = y - h;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
}

/* Bounds handler for the image item */
static void
image_item_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	ImageItem *ii;
	ImageItemPrivate *priv;

	ii = IMAGE_ITEM (item);
	priv = ii->priv;

	*x1 = 0.0;
	*y1 = 0.0;
	*x2 = priv->uwidth;
	*y2 = priv->uheight;
}
