#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <libgnome/gnome-macros.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "eog-canvas-pixbuf.h"

/* All compute_* functions are verbatim copied from
 * gnome-canvas-pixbuf.c from the libgnomecanvas library, Author:
 * Federico Mena-Quintero <federico@gimp.org>
 */

/* Computes the amount by which the unit horizontal and vertical vectors will be
 * scaled by an affine transformation.
 */
static void
compute_xform_scaling (double *affine, ArtPoint *i_c, ArtPoint *j_c)
{
	ArtPoint orig, orig_c;
	ArtPoint i, j;

	/* Origin */

	orig.x = 0.0;
	orig.y = 0.0;
	art_affine_point (&orig_c, &orig, affine);

	/* Horizontal and vertical vectors */

	i.x = 1.0;
	i.y = 0.0;
	art_affine_point (i_c, &i, affine);
	i_c->x -= orig_c.x;
	i_c->y -= orig_c.y;

	j.x = 0.0;
	j.y = 1.0;
	art_affine_point (j_c, &j, affine);
	j_c->x -= orig_c.x;
	j_c->y -= orig_c.y;
}

/* computes the addtional resolution dependent affine needed to
 * fit the image within its viewport defined by x,y,width and height
 * args
 */
static void
compute_viewport_affine (GnomeCanvasPixbuf *gcp, double *viewport_affine, double *i2c)
{
	GdkPixbuf *pixbuf;
	ArtPoint i_c, j_c;
	gboolean width_set;
	gboolean height_set;
	gboolean width_in_pixels;
	gboolean height_in_pixels;
	gboolean x_in_pixels;
	gboolean y_in_pixels;
	GtkAnchorType anchor;
	double width;
	double height;
	double i_len, j_len;
	double si_len, sj_len;
	double ti_len, tj_len;
	double scale[6], translate[6];
	double w, h;
	double x, y;

	/* obtain required internal variables from the GnomeCanvasPixbuf parent */
        g_object_get (G_OBJECT (gcp), 
		      "pixbuf", &pixbuf, 
		      "width_set", &width_set,
		      "height_set", &height_set,
		      "width", &width,
		      "height", &height,
		      "x", &x,
		      "y", &y,
		      "width_in_pixels", &width_in_pixels,
		      "height_in_pixels", &height_in_pixels,
		      "x_in_pixels", &x_in_pixels,
		      "y_in_pixels", &y_in_pixels,
		      "anchor", &anchor,
		      NULL);

	/* Compute scaling vectors and required width/height */

	compute_xform_scaling (i2c, &i_c, &j_c);

	i_len = sqrt (i_c.x * i_c.x + i_c.y * i_c.y);
	j_len = sqrt (j_c.x * j_c.x + j_c.y * j_c.y);

	if (width_set)
		w = width;
	else
		w = gdk_pixbuf_get_width (pixbuf);

	if (height_set)
		h = height;
	else
		h = gdk_pixbuf_get_height (pixbuf);

	/* Convert i_len and j_len into scaling factors */

	if (width_in_pixels) {
		if (i_len > GNOME_CANVAS_EPSILON)
			si_len = 1.0 / i_len;
		else
			si_len = 0.0;
	} else
		si_len = 1.0;

	si_len *= w / gdk_pixbuf_get_width (pixbuf);

	if (height_in_pixels) {
		if (j_len > GNOME_CANVAS_EPSILON)
			sj_len = 1.0 / j_len;
		else
			sj_len = 0.0;
	} else
		sj_len = 1.0;

	sj_len *= h / gdk_pixbuf_get_height (pixbuf);

	/* Calculate translation offsets */

	if (x_in_pixels) {
		if (i_len > GNOME_CANVAS_EPSILON)
			ti_len = 1.0 / i_len;
		else
			ti_len = 0.0;
	} else
		ti_len = 1.0;

	switch (anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		ti_len *= x;
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		ti_len *= (x - (w/ 2));
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		ti_len *= (x - w);
		break;

        default:
                break;
	}

	if (y_in_pixels) {
		if (j_len > GNOME_CANVAS_EPSILON)
			tj_len = 1.0 / j_len;
		else
			tj_len = 0.0;
	} else
		tj_len = 1.0;

	switch (anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		tj_len *= y;
		break;
		
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		tj_len *= y - (h / 2);
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		tj_len *= y - h;
		break;

        default:
                break;
	}

	/* Compute the final affine */

	art_affine_scale (scale, si_len, sj_len);
	art_affine_translate (translate, ti_len, tj_len);
  	art_affine_multiply (viewport_affine, scale, translate); 

	g_object_unref (pixbuf);
}

/* Computes the affine transformation with which the pixbuf needs to be
 * transformed to render it on the canvas.  This is not the same as the
 * item_to_canvas transformation because we may need to scale the pixbuf
 * by some other amount.
 */
static void
compute_render_affine (GnomeCanvasPixbuf *gcp, double *ra, double *i2c)
{
	double va[6];

	compute_viewport_affine (gcp, va, i2c);
	art_affine_multiply (ra, va, i2c);
}


/* Point handler for the pixbuf canvas item. Verbatim copy of
 * gnome-canvas-pixbuf.c:gnome_canvas_pixbuf_point except that the
 * transparency check is removed.*/
/* Author: Federico Mena-Quintero <federico@gimp.org> */
static double
eog_canvas_pixbuf_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
			 GnomeCanvasItem **actual_item)
{
	GnomeCanvasPixbuf *gcp;
	double i2c[6], render_affine[6], inv[6];
	ArtPoint c, p;
	int px, py;
	double no_hit;
	double result = 0.0;
	GdkPixbuf *pixbuf;

	gcp = GNOME_CANVAS_PIXBUF (item);

	g_object_get (G_OBJECT (gcp), "pixbuf", &pixbuf, NULL);

	*actual_item = item;

	no_hit = item->canvas->pixels_per_unit * 2 + 10;

	if (!pixbuf)
		return no_hit;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);
	art_affine_invert (inv, render_affine);

	c.x = cx;
	c.y = cy;
	art_affine_point (&p, &c, inv);
	px = p.x;
	py = p.y;

	if (px < 0 || px >= gdk_pixbuf_get_width (pixbuf) ||
	    py < 0 || py >= gdk_pixbuf_get_height (pixbuf))
	{
		result = no_hit;
	}

	g_object_unref (pixbuf);

	return result;
}

static void
eog_canvas_pixbuf_finalize (GObject *object)
{
}

static void
eog_canvas_pixbuf_dispose (GObject *object)
{
}

static void
eog_canvas_pixbuf_instance_init (EogCanvasPixbuf *obj)
{
}



static void 
eog_canvas_pixbuf_class_init (EogCanvasPixbufClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;
	GnomeCanvasItemClass *item_klass = (GnomeCanvasItemClass*) klass;

	object_class->finalize = eog_canvas_pixbuf_finalize;
	object_class->dispose = eog_canvas_pixbuf_dispose;

	item_klass->point = eog_canvas_pixbuf_point;
}


GNOME_CLASS_BOILERPLATE (EogCanvasPixbuf,
			 eog_canvas_pixbuf,
			 GnomeCanvasPixbuf,
			 GNOME_TYPE_CANVAS_PIXBUF)

