/* Eye of Gnome - default item factory for icons
 *
 * Copyright (C) 2000-2001 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gdk/gdkx.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-canvas-text.h>
#include "eog-item-factory-simple.h"
#include "eog-collection-model.h"
#include "cimage.h"



/* Private part of the EogItemFactorySimple structure */
struct _EogItemFactorySimplePrivate {
	/* item metrics */
	EogSimpleMetrics *metrics;

	/* default pixmap if the image is not loaded so far */
	GdkPixbuf *dummy;
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
} IconItem;



static void eog_item_factory_simple_class_init (EogItemFactorySimpleClass *class);
static void eog_item_factory_simple_init (EogItemFactorySimple *factory);
static void eog_item_factory_simple_destroy (GtkObject *object);
static void eog_item_factory_simple_finalize (GtkObject *object);

static GnomeCanvasItem *ii_factory_create_item (EogItemFactory *factory,
						GnomeCanvasGroup *parent, guint id);
static void ii_factory_update_item (EogItemFactory *factory,
				    EogCollectionModel *model, 
				    GnomeCanvasItem *item);
static void ii_factory_get_item_size (EogItemFactory *factory,
				      gint *width, gint *height);

static EogItemFactoryClass *parent_class;



/**
 * eog_item_factory_simple_get_type:
 * @void:
 *
 * Registers the #EogItemFactorySimple class if necessary, and returns the type
 * ID associated to it.
 *
 * Return value: The type ID of the #EogItemFactorySimple class.
 **/
GtkType
eog_item_factory_simple_get_type (void)
{
	static GtkType ii_factory_type = 0;

	if (!ii_factory_type) {
		static const GtkTypeInfo ii_factory_info = {
			"EogItemFactorySimple",
			sizeof (EogItemFactorySimple),
			sizeof (EogItemFactorySimpleClass),
			(GtkClassInitFunc) eog_item_factory_simple_class_init,
			(GtkObjectInitFunc) eog_item_factory_simple_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ii_factory_type = gtk_type_unique (eog_item_factory_get_type (),
						   &ii_factory_info);
	}

	return ii_factory_type;
}

/* Class initialization function for the icon list item factory */
static void
eog_item_factory_simple_class_init (EogItemFactorySimpleClass *class)
{
	GtkObjectClass *object_class;
        EogItemFactoryClass *ei_factory_class;

	object_class = (GtkObjectClass *) class;
	ei_factory_class = (EogItemFactoryClass *) class;

	parent_class = gtk_type_class (eog_item_factory_get_type ());

	object_class->destroy = eog_item_factory_simple_destroy;
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
eog_item_factory_simple_init (EogItemFactorySimple *factory)
{
	EogItemFactorySimplePrivate *priv;
	EogSimpleMetrics *metrics;
	char *dummy_file;

	priv = g_new0 (EogItemFactorySimplePrivate, 1);
	factory->priv = priv;

	/* load dummy pixmap file */
	dummy_file = gnome_pixmap_file ("gnome-eog.png");
	priv->dummy = gdk_pixbuf_new_from_file (dummy_file);
	g_free (dummy_file);

	/* set semi sane default values for item metrics */
	metrics = g_new0 (EogSimpleMetrics, 1);
	metrics->twidth = 100;
	metrics->theight = 100;
	metrics->cspace = 10;
	metrics->border = 5;
	priv->metrics = metrics;
}

/* Destroy handler for the icon list item factory */
static void
eog_item_factory_simple_destroy (GtkObject *object)
{
	EogItemFactorySimple *factory;
	EogItemFactorySimplePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (object));

	factory = EOG_ITEM_FACTORY_SIMPLE (object);
	priv = factory->priv;
	
	if (priv->dummy)
		gdk_pixbuf_unref (priv->dummy);
	priv->dummy = NULL;
	
	if (priv->metrics)
		g_free (priv->metrics);
	priv->metrics = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
eog_item_factory_simple_finalize (GtkObject *object)
{
	EogItemFactorySimple *factory;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (object));

	factory = EOG_ITEM_FACTORY_SIMPLE (object);
	g_free (factory->priv);

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Default signal handlers */

/* Called when an icon's main group is destroyed */
static void
icon_destroyed (GtkObject *object, gpointer data)
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
	gtk_object_set_data (GTK_OBJECT (icon->item), "IconItem", icon);
	gtk_signal_connect (GTK_OBJECT (icon->item), "destroy",
			    icon_destroyed,
			    icon);

	/* Image */
	icon->image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					     GNOME_TYPE_CANVAS_PIXBUF,
					     NULL);
	/* Caption */
	icon->caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					       GNOME_TYPE_CANVAS_TEXT,
					       NULL);

	/* Border */
	icon->border = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					      GNOME_TYPE_CANVAS_RECT,
					      NULL);
	return icon->item;
}

/* Converts a GdkColor's RGB fields into a packed color integer */
static guint
color_to_rgba (GdkColor *color)
{
	return (((color->red & 0xff00) << 16)
		| ((color->green & 0xff00) << 8)
		| (color->blue & 0xff00)
		| 0xff);
}

/* Shrink the string until its pixel width is <= width */
static char*
shrink_to_width (char *str, GdkFont *font,  int width)
{
	char *buffer;
	char *result;
	const char *dots = "...";
	int dot_width;
	int len; 

	buffer =  g_strdup (str);
	len = strlen (buffer);	
	g_assert (len > 0);

	dot_width = gdk_string_width (font, dots);
	if (dot_width > width) {
		g_warning ("Couldn't shrink %s to width %i.\n", str, width);
		return NULL;
	}

	while (gdk_string_width (font, buffer) > (width - dot_width))
	{
		--len;
		if(len > 0)
			buffer[len] = '\0';
		else
			break;
	}

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
	GdkFont *font;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_SIMPLE (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	ii_factory = EOG_ITEM_FACTORY_SIMPLE (factory);
	priv = ii_factory->priv;
	metrics = priv->metrics;
	
	icon = gtk_object_get_data (GTK_OBJECT (item), "IconItem");
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
		gdk_pixbuf_ref (thumb);
		image_w = gdk_pixbuf_get_width (thumb);
		image_h = gdk_pixbuf_get_height (thumb);
	}
	g_assert (thumb != NULL);
	
	/* Compute caption size; the font comes from the widget style */
	style = gtk_widget_get_style (GTK_WIDGET (icon->caption->canvas));
	g_assert (style != NULL);
	font = style->font;
	g_assert (font != NULL);

	if (cimage_has_caption (cimage)) {
		caption = cimage_get_caption (cimage);
		caption_w = gdk_string_width (font, caption);
		caption_h = gdk_string_height (font, caption);
	} else {
		gchar *path;
		path = cimage_get_path (cimage);
		caption = g_strdup (g_basename (path));
		g_free (path);
		
		if (caption) {
			caption_w = gdk_string_width (font, caption);
			caption_h = font->ascent + font->descent;
			if (caption_w > metrics->twidth) {
				caption = shrink_to_width (caption, 
							   font, 
							   metrics->twidth);
				caption_w = gdk_string_width (font, caption);
			}
			cimage_set_caption (cimage, caption);
			
		} else
			caption_w = caption_h = 0;
	} 

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

	gdk_pixbuf_unref (thumb);


	/* Configure caption */

	caption_x = metrics->border + (metrics->twidth - caption_w) / 2;
	caption_y = metrics->border + metrics->theight + metrics->cspace;
	g_print ("caption_y: %d\n", caption_y);

	gnome_canvas_item_set (icon->caption,
			       "text", caption,
			       "font_gdk", font,
			       "anchor", GTK_ANCHOR_NW,
			       "x", (double) caption_x,
			       "y", (double) caption_y,
			       "fill_color_rgba", color_to_rgba (&style->fg[GTK_STATE_NORMAL]),
			       NULL);
	g_free (caption);

	/* configure border */
	ii_factory_get_item_size (factory, &item_w, &item_h);
	gnome_canvas_item_set (icon->border,
			       "x1", 0.0,
			       "y1", 0.0,
			       "x2", (double) item_w,
			       "y2", (double) item_h,
			       "fill_color", NULL,
			       "outline_color", "DarkBlue",
			       "width_pixels", 2,
			       NULL);
	
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



/* Exported functions */

EogItemFactorySimple *
eog_item_factory_simple_new (void)
{
	return EOG_ITEM_FACTORY_SIMPLE (gtk_type_new (eog_item_factory_simple_get_type ()));
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
