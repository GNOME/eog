/* Eye of Gnome - default item factory for icons
 *
 * Copyright (C) 2000-2001 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <pango/pango-layout.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-macros.h>
#include <libgnomecanvas/gnome-canvas-pixbuf.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libgnomecanvas/gnome-canvas-text.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include "eog-item-factory-simple.h"
#include "eog-collection-model.h"
#include "cimage.h"
#include <math.h>



/* Private part of the EogItemFactorySimple structure */
struct _EogItemFactorySimplePrivate {
	/* item metrics */
	EogSimpleMetrics *metrics;

	/* default pixmap if the image is not loaded so far */
	GdkPixbuf *dummy;

	GdkPixbuf *shaded_bgr;
};



/* Icon item structure */
typedef struct {
	/* unique id */
	guint id;

	/* Base group */
	GnomeCanvasItem *item;

	/* Icon image and its selection rectangle */
	GnomeCanvasItem *image;

	/* Caption and its selection and focus rectangles */
	GnomeCanvasItem *caption;

	/* border */
	GnomeCanvasItem *border;

	/* image background */
	GnomeCanvasItem *bgr;
} IconItem;



static void eog_item_factory_simple_class_init (EogItemFactorySimpleClass *class);
static void eog_item_factory_simple_instance_init (EogItemFactorySimple *factory);
static void eog_item_factory_simple_dispose (GObject *object);
static void eog_item_factory_simple_finalize (GObject *object);
static EogItemFactorySimple* eog_item_factory_simple_construct (EogItemFactorySimple *factory);

static GnomeCanvasItem *ii_factory_create_item (EogItemFactory *factory,
						GnomeCanvasGroup *parent, guint id);
static void ii_factory_update_item (EogItemFactory *factory,
				    EogCollectionModel *model, 
				    GnomeCanvasItem *item);
static void ii_factory_get_item_size (EogItemFactory *factory,
				      gint *width, gint *height);

GNOME_CLASS_BOILERPLATE (EogItemFactorySimple, eog_item_factory_simple,
			 EogItemFactory, EOG_TYPE_ITEM_FACTORY);

/* Class initialization function for the icon list item factory */
static void
eog_item_factory_simple_class_init (EogItemFactorySimpleClass *class)
{
	GObjectClass *object_class;
        EogItemFactoryClass *ei_factory_class;

	object_class = (GObjectClass *) class;
	ei_factory_class = (EogItemFactoryClass *) class;

	object_class->dispose = eog_item_factory_simple_dispose;
	object_class->finalize = eog_item_factory_simple_finalize;

	ei_factory_class->create_item = ii_factory_create_item;
	ei_factory_class->update_item = ii_factory_update_item;
	ei_factory_class->get_item_size = ii_factory_get_item_size;
}

#if 0
#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01, };
#endif

/* Object initialization function for the icon list item factory */
static void
eog_item_factory_simple_instance_init (EogItemFactorySimple *factory)
{
	EogItemFactorySimplePrivate *priv;
	EogSimpleMetrics *metrics;

	priv = g_new0 (EogItemFactorySimplePrivate, 1);

	metrics = g_new0 (EogSimpleMetrics, 1);
	metrics->twidth = 92;
	metrics->theight = 69;
	metrics->cspace = 10;
	metrics->border = 5;
	priv->metrics = metrics;

	factory->priv = priv;
}

/* Destroy handler for the icon list item factory */
static void
eog_item_factory_simple_dispose (GObject *object)
{
	EogItemFactorySimple *factory;
	EogItemFactorySimplePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (object));

	factory = EOG_ITEM_FACTORY_SIMPLE (object);
	priv = factory->priv;
	
	if (priv->dummy)
		g_object_unref (priv->dummy);
	priv->dummy = NULL;
	
	if (priv->metrics)
		g_free (priv->metrics);
	priv->metrics = NULL;

	if (priv->shaded_bgr)
		g_object_unref (priv->shaded_bgr);
	priv->shaded_bgr = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_item_factory_simple_finalize (GObject *object)
{
	EogItemFactorySimple *factory;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (object));

	factory = EOG_ITEM_FACTORY_SIMPLE (object);
	g_free (factory->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object)); 
}



/* Default signal handlers */

/* Called when an icon's main group is destroyed */
static void
icon_destroyed (GObject *object, gpointer data)
{
	IconItem *icon;

	icon = (IconItem*)data;
	g_free (icon);
}

/* Create_item handler for the icon list item factory */
static GnomeCanvasItem *
ii_factory_create_item (EogItemFactory *factory, 
			GnomeCanvasGroup *parent, guint unique_id)
{
	IconItem *icon;

	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (factory), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	icon = g_new (IconItem, 1);

	icon->id = unique_id;
	icon->item = gnome_canvas_item_new (parent,
					    GNOME_TYPE_CANVAS_GROUP,
					    "x", 0.0,
					    "y", 0.0,
					    NULL);
	g_object_set_data (G_OBJECT (icon->item), "IconItem", icon);
	g_signal_connect (G_OBJECT (icon->item), "destroy",
			  G_CALLBACK (icon_destroyed),
			  icon);

	/* Border */
	icon->border = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					      GNOME_TYPE_CANVAS_RECT,
					      NULL);
	
	/* background */
	icon->bgr = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					   GNOME_TYPE_CANVAS_PIXBUF,
					   NULL);

	/* Image */
	icon->image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					     GNOME_TYPE_CANVAS_PIXBUF,
					     NULL);
	/* Caption */
	icon->caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					       GNOME_TYPE_CANVAS_TEXT,
					       NULL);

	return icon->item;
}

#if 0
/* Converts a GdkColor's RGB fields into a packed color integer */
static guint
color_to_rgba (GdkColor *color)
{
	return (((color->red & 0xff00) << 16)
		| ((color->green & 0xff00) << 8)
		| (color->blue & 0xff00)
		| 0xff);
}
#endif 

static void
create_shaded_background (GdkPixbuf *pbf) 
{
	int width, height;
	double dl;       
	double tmp;
	int x, y;
	int n_cols;    
	int cvd = 5;        /* color value decrement */
	int light = 230;     /* gray value */
	int dark  = 150;     /* gray value */
	int ppc;             /* pixels per color */
	guchar *buf;

	g_return_if_fail (pbf != NULL);
	
	width =  gdk_pixbuf_get_width (pbf);
	height = gdk_pixbuf_get_height (pbf);
	buf = gdk_pixbuf_get_pixels (pbf);

	tmp = width*width + height*height;
	dl = sqrt (tmp);
	
	n_cols = (light - dark)/cvd;
	ppc = dl / n_cols;

	for (y = 0; y < height; y++) {
		gint color;
		gint ppcy; /* pixels per color yet */
		
		color = light - (cvd * (y/ppc));
		ppcy =  y % ppc;
 
		for (x = 0; x < width; x++) {
			*buf = color;
			*(buf+1) = color;
			*(buf+2) = color;
			buf += 3;
			
			ppcy++;
			if (ppcy == ppc) {
				ppcy = 0;
				color = color - cvd;
			}
		}
	}
}

/* Shrink the string until its pixel width is <= width */
static char*
shrink_to_width (char *str, PangoLayout *layout,  int width)
{
	char *buffer;
	char *result;
	const char *dots = "...";
	int dot_width;
	int len; 
	int px_width, px_height;

	buffer =  g_strdup (str);
	len = strlen (buffer);	
	g_assert (len > 0);

	/* FIXME: doesn't work with unicode */

	pango_layout_set_text (layout, dots, -1);
	pango_layout_get_pixel_size (layout, &dot_width, &px_height);

	if (dot_width > width) {
		g_warning ("Couldn't shrink %s to width %i.\n", str, width);
		return NULL;
	}

	do {
		--len;
		if(len > 0)
			buffer[len] = '\0';
		else
			break;
		pango_layout_set_text (layout, buffer, -1);
		pango_layout_get_pixel_size (layout, &px_width, &px_height);
	} while (px_width > (width - dot_width));

	result = g_strconcat (buffer, dots, NULL);
	g_free (buffer);	
	
	return result;
}


/* Configure_item handler for the icon list item factory */
static void
ii_factory_update_item (EogItemFactory *factory, 
			EogCollectionModel *model,
			GnomeCanvasItem *item)
{
	EogItemFactorySimple *ii_factory;
	GdkPixbuf *thumb;
	EogItemFactorySimplePrivate *priv;
	EogSimpleMetrics *metrics;
	CImage *cimage = NULL;
	IconItem *icon;
	int image_w, image_h;
	int image_x, image_y;
	int caption_w, caption_h;
	int caption_x, caption_y;
	int item_w, item_h;
	gchar *caption;

	GtkStyle *style;
	PangoLayout  *p_layout;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	ii_factory = EOG_ITEM_FACTORY_SIMPLE (factory);
	priv = ii_factory->priv;
	metrics = priv->metrics;
	
	icon = g_object_get_data (G_OBJECT (item), "IconItem");
	g_assert (icon != NULL);

	cimage = eog_collection_model_get_image (model, icon->id);
	g_assert (cimage != NULL);
	
	/* obtain thumbnail image */
	if (cimage_has_thumbnail (cimage)) {
		thumb = cimage_get_thumbnail (cimage);
		image_w = gdk_pixbuf_get_width (thumb);
		image_h = gdk_pixbuf_get_height (thumb);
	} else {
		thumb = priv->dummy;
		g_object_ref (thumb);
		image_w = gdk_pixbuf_get_width (thumb);
		image_h = gdk_pixbuf_get_height (thumb);
	}
	g_assert (thumb != NULL);
	
	/* Compute caption size; the font comes from the widget style */
	style = gtk_widget_get_style (GTK_WIDGET (icon->caption->canvas));
	g_assert (style != NULL);

	p_layout = pango_layout_new (NULL);
	pango_layout_set_font_description (p_layout, style->font_desc);

	g_assert (p_layout != NULL);

	if (cimage_has_caption (cimage)) {
		caption = cimage_get_caption (cimage);
		pango_layout_set_text (p_layout, caption, -1);
		pango_layout_get_pixel_size (p_layout, &caption_w, &caption_h);
	} else {
		GnomeVFSURI *uri;
		uri = cimage_get_uri (cimage);
		caption = g_strdup (gnome_vfs_uri_get_path (uri));
	        gnome_vfs_uri_unref (uri);
		
		if (caption) {
			pango_layout_set_text (p_layout, caption, -1);
			pango_layout_get_pixel_size (p_layout, &caption_w, &caption_h);

			if (caption_w > metrics->twidth) {
				caption = shrink_to_width (caption, 
							   p_layout, 
							   metrics->twidth);
				pango_layout_get_pixel_size (p_layout, &caption_w, &caption_h);
			}
			cimage_set_caption (cimage, caption);
			
		} else
			caption_w = caption_h = 0;
	} 
	g_object_unref (p_layout);

	/* Configure image */
	image_x = metrics->border + (metrics->twidth - image_w) / 2;
	image_y = metrics->border + (metrics->theight - image_h) / 2;

	gnome_canvas_item_set (icon->image,
			       "pixbuf", thumb,
			       "x", (double) image_x,
			       "y", (double) image_y,
			       "width_set", FALSE,
			       "height_set", FALSE,
			       NULL);

	g_object_unref (thumb);


	/* Configure caption */

	caption_x = metrics->border + (metrics->twidth - caption_w) / 2;
	caption_y = metrics->border + metrics->theight + metrics->cspace;

	gnome_canvas_item_set (icon->caption,
			       "text", caption,
			       "font_desc", style->font_desc,
			       "anchor", GTK_ANCHOR_NW,
			       "x", (double) caption_x,
			       "y", (double) caption_y,
			       "fill_color", "White",
			       NULL);
	g_free (caption);

	/* configure border */
	ii_factory_get_item_size (factory, &item_w, &item_h);

	if (cimage_is_selected (cimage))
		gnome_canvas_item_set (icon->border,
				       "x1", 0.0,
				       "y1", 0.0,
				       "x2", (double) item_w,
				       "y2", (double) item_h,
				       "fill_color", NULL,
				       "outline_color", "Red",
				       "fill_color", "Black",
				       "width_pixels", 2,
				       NULL);
	else 
		gnome_canvas_item_set (icon->border,
				       "x1", 0.0,
				       "y1", 0.0,
				       "x2", (double) item_w,
				       "y2", (double) item_h,
				       "fill_color", NULL,
				       "outline_color", "DarkBlue",
				       "fill_color", "Black",
				       "width_pixels", 2,
				       NULL);
	

	/* configure background */
	{
		double x1, y1, x2, y2;
		x1 = metrics->border;
		y1 = metrics->border;
#if 0
		x2 = x1 + metrics->twidth;
		y2 = y1 + metrics->theight;
#endif

		gnome_canvas_item_set (icon->bgr,
				       "pixbuf", priv->shaded_bgr,
				       "x", (double) x1,
				       "y", (double) y1,
				       "width_set", FALSE,
				       "height_set", FALSE,
				       NULL);
	}
}

/* Get_item_size handler for the icon list item factory */
static void
ii_factory_get_item_size (EogItemFactory *factory,
			  gint *width, gint *height)
{
	EogItemFactorySimple *ii_factory;
	EogSimpleMetrics *metrics;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (factory));

	ii_factory = EOG_ITEM_FACTORY_SIMPLE (factory);
	metrics = ii_factory->priv->metrics;

	*width = metrics->twidth + 2*metrics->border;
	*height = metrics->theight + 2*metrics->border + 
		metrics->cspace + 12 /* font height; FIXME don't hardcode this */;
}



static void
shade_bgr_destroy_notify_cb (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

static EogItemFactorySimple*
eog_item_factory_simple_construct (EogItemFactorySimple *factory)
{
	EogItemFactorySimplePrivate *priv;
	char *dummy_file;
	guchar *buffer;

	priv = factory->priv;

	/* load dummy pixmap file */
	dummy_file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						"gnome-eog.png", TRUE, NULL);
	priv->dummy = gdk_pixbuf_new_from_file (dummy_file, NULL);
	g_free (dummy_file);

	buffer = g_new0 (guchar, 3*priv->metrics->twidth * priv->metrics->theight);
	priv->shaded_bgr = gdk_pixbuf_new_from_data (buffer,
						     GDK_COLORSPACE_RGB,
						     FALSE,
						     8, 
						     priv->metrics->twidth,
						     priv->metrics->theight,
						     3 * priv->metrics->twidth,
						     (GdkPixbufDestroyNotify) shade_bgr_destroy_notify_cb,
						     NULL);
	g_object_ref (priv->shaded_bgr);
	create_shaded_background (priv->shaded_bgr);

	return factory;
}


EogItemFactorySimple *
eog_item_factory_simple_new (void)
{
	EogItemFactorySimple *factory;

	factory = EOG_ITEM_FACTORY_SIMPLE (g_object_new (EOG_TYPE_ITEM_FACTORY_SIMPLE, NULL));
	return eog_item_factory_simple_construct (factory);
}

/**
 * eog_item_factory_simple_set_item_metrics:
 * @factory: An icon list item factory.
 * @item_width: Total width of items, in pixels.
 * @item_height: Total height of items, in pixels.
 * @image_width: Maximum width of images for icons, in pixels.
 * @image_height: Maximum height of images for icons, in pixels.
 *
 * Sets the metrics of the items that will be created in the future by an icon
 * list item factory.  This includes the total size of items and the maximum
 * size of icon images.
 **/
void
eog_item_factory_simple_set_metrics (EogItemFactorySimple *factory,
				     EogSimpleMetrics *metrics)
{
	EogItemFactorySimplePrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (factory));
	g_return_if_fail (metrics->twidth > 0);
	g_return_if_fail (metrics->theight > 0);
	g_return_if_fail (metrics->cspace >= 0);
	g_return_if_fail (metrics->border >= 0);

	priv = factory->priv;

	if (priv->metrics)
		g_free (priv->metrics);
	
	priv->metrics = metrics;

	eog_item_factory_configuration_changed (EOG_ITEM_FACTORY (factory));
}

EogSimpleMetrics* 
eog_item_factory_simple_get_metrics (EogItemFactorySimple *factory)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (factory), NULL);

	return factory->priv->metrics;
}
