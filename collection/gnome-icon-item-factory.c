/* GNOME libraries - default item factory for icon lists
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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
#include "gnome-icon-item-factory.h"
#include "eog-image-list-model.h"
#include "cimage.h"



/* Private part of the GnomeIconItemFactory structure */
typedef struct {
	/* Size of items */
	int item_width;
	int item_height;

	/* Size of icon images */
	int image_width;
	int image_height;

	/* Stipple pattern for selection and focus */
	GdkBitmap *stipple;
	GdkPixbuf *dummy;
} IIFactoryPrivate;



/* Icon item structure */
typedef struct {
	/* Base group */
	GnomeCanvasItem *item;

	/* Icon image and its selection rectangle */
	GnomeCanvasItem *image;
	GnomeCanvasItem *sel_image;

	/* Caption and its selection and focus rectangles */
	GnomeCanvasItem *caption;
	GnomeCanvasItem *sel_caption;
	GnomeCanvasItem *focus_caption;
} IconItem;



static void gnome_icon_item_factory_class_init (GnomeIconItemFactoryClass *class);
static void gnome_icon_item_factory_init (GnomeIconItemFactory *factory);
static void gnome_icon_item_factory_destroy (GtkObject *object);

static GnomeCanvasItem *ii_factory_create_item (GnomeListItemFactory *factory,
						GnomeCanvasGroup *parent);
static void ii_factory_configure_item (GnomeListItemFactory *factory, GnomeCanvasItem *item,
				       GnomeListModel *model, guint n,
				       gboolean is_selected, gboolean is_focused);
static void ii_factory_get_item_size (GnomeListItemFactory *factory, GnomeCanvasItem *item,
				      GnomeListModel *model, guint n,
				      gint *width, gint *height);

static GnomeListItemFactoryClass *parent_class;



/**
 * gnome_icon_item_factory_get_type:
 * @void:
 *
 * Registers the #GnomeIconItemFactory class if necessary, and returns the type
 * ID associated to it.
 *
 * Return value: The type ID of the #GnomeIconItemFactory class.
 **/
GtkType
gnome_icon_item_factory_get_type (void)
{
	static GtkType ii_factory_type = 0;

	if (!ii_factory_type) {
		static const GtkTypeInfo ii_factory_info = {
			"GnomeIconItemFactory",
			sizeof (GnomeIconItemFactory),
			sizeof (GnomeIconItemFactoryClass),
			(GtkClassInitFunc) gnome_icon_item_factory_class_init,
			(GtkObjectInitFunc) gnome_icon_item_factory_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ii_factory_type = gtk_type_unique (gnome_list_item_factory_get_type (),
						   &ii_factory_info);
	}

	return ii_factory_type;
}

/* Class initialization function for the icon list item factory */
static void
gnome_icon_item_factory_class_init (GnomeIconItemFactoryClass *class)
{
	GtkObjectClass *object_class;
	GnomeListItemFactoryClass *li_factory_class;

	object_class = (GtkObjectClass *) class;
	li_factory_class = (GnomeListItemFactoryClass *) class;

	parent_class = gtk_type_class (gnome_list_item_factory_get_type ());

	object_class->destroy = gnome_icon_item_factory_destroy;

	li_factory_class->create_item = ii_factory_create_item;
	li_factory_class->configure_item = ii_factory_configure_item;
	li_factory_class->get_item_size = ii_factory_get_item_size;
}

#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01, };

/* Object initialization function for the icon list item factory */
static void
gnome_icon_item_factory_init (GnomeIconItemFactory *factory)
{
	IIFactoryPrivate *priv;
	char *dummy_file;

	priv = g_new0 (IIFactoryPrivate, 1);
	factory->priv = priv;

	priv->stipple = gdk_bitmap_create_from_data (GDK_ROOT_PARENT (),
						     gray50_bits,
 						     gray50_width, gray50_height);
       
	dummy_file = gnome_pixmap_file ("gnome-eog.png");
	priv->dummy = gdk_pixbuf_new_from_file (dummy_file);
	g_free (dummy_file);
}

/* Destroy handler for the icon list item factory */
static void
gnome_icon_item_factory_destroy (GtkObject *object)
{
	GnomeIconItemFactory *factory;
	IIFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (object));

	factory = GNOME_ICON_ITEM_FACTORY (object);
	priv = factory->priv;

	gdk_bitmap_unref (priv->stipple);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Default signal handlers */

/* Called when an icon's main group is destroyed */
static void
icon_destroyed (GtkObject *object, gpointer data)
{
	IconItem *icon;

	icon = data;
	g_free (icon);
}

/* Create_item handler for the icon list item factory */
static GnomeCanvasItem *
ii_factory_create_item (GnomeListItemFactory *factory, GnomeCanvasGroup *parent)
{
	IconItem *icon;

	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	icon = g_new (IconItem, 1);

	icon->item = gnome_canvas_item_new (parent,
					    GNOME_TYPE_CANVAS_GROUP,
					    NULL);
	gtk_object_set_data (GTK_OBJECT (icon->item), "IconItem", icon);
	gtk_signal_connect (GTK_OBJECT (icon->item), "destroy",
			    icon_destroyed,
			    icon);

	/* Image */

	icon->image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					     GNOME_TYPE_CANVAS_PIXBUF,
					     NULL);

	icon->sel_image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						 GNOME_TYPE_CANVAS_RECT,
						 NULL);
	gnome_canvas_item_hide (icon->sel_image);

	/* Caption */

	icon->sel_caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						   GNOME_TYPE_CANVAS_RECT,
						   NULL);
	gnome_canvas_item_hide (icon->sel_caption);

	icon->caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					       GNOME_TYPE_CANVAS_TEXT,
					       NULL);

	icon->focus_caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						     GNOME_TYPE_CANVAS_RECT,
						     NULL);
	gnome_canvas_item_hide (icon->focus_caption);

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
ii_factory_configure_item (GnomeListItemFactory *factory, GnomeCanvasItem *item,
			   GnomeListModel *model, guint n,
			   gboolean is_selected, gboolean is_focused)
{
	GnomeIconItemFactory *ii_factory;
	GdkPixbuf *thumb;
	IIFactoryPrivate *priv;
	CImage *cimage = NULL;
	IconItem *icon;
	int image_w, image_h;
	int image_x, image_y;
	int caption_w, caption_h;
	int caption_x, caption_y;
	gchar *caption;
	guint sel_color;
	GtkStyle *style;
	GdkFont *font;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_MODEL (model));
	g_return_if_fail (n < gnome_list_model_get_length (model));

	ii_factory = GNOME_ICON_ITEM_FACTORY (factory);
	priv = ii_factory->priv;
	
	icon = gtk_object_get_data (GTK_OBJECT (item), "IconItem");
	g_assert (icon != NULL);

	gnome_icon_list_model_get_icon (GNOME_ICON_LIST_MODEL (model), n,
					&cimage);
	
	/* Compute thumbnail image */
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
			if (caption_w > priv->item_width) {
				caption = shrink_to_width (caption, font, priv->item_width);
				caption_w = gdk_string_width (font, caption);
			}
			cimage_set_caption (cimage, caption);
			
		} else
			caption_w = caption_h = 0;
	} 

	/* Configure image */

	image_x = (priv->item_width - image_w) / 2;
	image_y = (priv->image_height - image_h) / 2;

	gnome_canvas_item_set (icon->image,
			       "pixbuf", thumb,
			       "x", (double) image_x,
			       "y", (double) image_y,
			       "width_set", FALSE,
			       "height_set", FALSE,
			       NULL);

	gdk_pixbuf_unref (thumb);


	/* Configure caption */

	caption_x = (priv->item_width - caption_w) / 2;
	caption_y = priv->image_height + 3;

	gnome_canvas_item_set (icon->caption,
			       "text", caption,
			       "font_gdk", font,
			       "anchor", GTK_ANCHOR_NW,
			       "x", (double) caption_x,
			       "y", (double) caption_y,
			       "fill_color_rgba", color_to_rgba (&style->fg[GTK_STATE_NORMAL]),
			       NULL);
	g_free (caption);

	/* Configure selection and focus */

	if (is_selected) {
		gnome_canvas_item_show (icon->sel_image);
		gnome_canvas_item_show (icon->sel_caption);

		sel_color = color_to_rgba (&style->bg[GTK_STATE_SELECTED]);

		gnome_canvas_item_set (icon->sel_image,
				       "x1", (double) image_x,
				       "y1", (double) image_y,
				       "x2", (double) (image_x + image_w - 1),
				       "y2", (double) (image_y + image_h - 1),
				       "fill_color_rgba", sel_color,
				       "fill_stipple", priv->stipple,
				       NULL);

		gnome_canvas_item_set (icon->sel_caption,
				       "x1", (double) caption_x,
				       "y1", (double) caption_y,
				       "x2", (double) (caption_x + caption_w - 1),
				       "y2", (double) (caption_y + caption_h - 1),
				       "fill_color_rgba", sel_color,
				       NULL);
	} else {
		gnome_canvas_item_hide (icon->sel_image);
		gnome_canvas_item_hide (icon->sel_caption);
	}

	if (is_focused) {
		gnome_canvas_item_show (icon->focus_caption);

		gnome_canvas_item_set (icon->focus_caption,
				       "x1", (double) (caption_x - 2),
				       "y1", (double) (caption_y - 2),
				       "x2", (double) (caption_x + caption_w - 1 + 2),
				       "y2", (double) (caption_y + caption_h - 1 + 2),
				       "width_pixels", 0,
				       "outline_color_rgba", 0x000000ff,
				       "outline_stipple", priv->stipple,
				       NULL);
	} else
		gnome_canvas_item_hide (icon->focus_caption);
}

/* Get_item_size handler for the icon list item factory */
static void
ii_factory_get_item_size (GnomeListItemFactory *factory, GnomeCanvasItem *item,
			  GnomeListModel *model, guint n,
			  gint *width, gint *height)
{
	GnomeIconItemFactory *ii_factory;
	IIFactoryPrivate *priv;

	ii_factory = GNOME_ICON_ITEM_FACTORY (factory);
	priv = ii_factory->priv;

	/* FIXME: return real canvas item size */

	*width = priv->item_width;
	*height = priv->item_height;
}



/* Exported functions */

GnomeIconItemFactory *
gnome_icon_item_factory_new (void)
{
	return GNOME_ICON_ITEM_FACTORY (gtk_type_new (gnome_icon_item_factory_get_type ()));
}

/**
 * gnome_icon_item_factory_set_item_metrics:
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
gnome_icon_item_factory_set_item_metrics (GnomeIconItemFactory *factory,
					  int item_width, int item_height,
					  int image_width, int image_height)
{
	IIFactoryPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory));
	g_return_if_fail (item_width > 0);
	g_return_if_fail (item_height > 0);
	g_return_if_fail (image_width <= item_width);
	g_return_if_fail (image_height <= item_height);

	priv = factory->priv;
	priv->item_width = item_width;
	priv->item_height = item_height;
	priv->image_width = image_width;
	priv->image_height = image_height;
}

/**
 * gnome_icon_item_factory_get_item_metrics:
 * @factory: An icon list item factory.
 * @item_width: Return value for the total width of items, in pixels.
 * @item_height: Return value for the total height of items, in pixels.
 * @image_width: Return value for the maximum width of icon images, in pixels.
 * @image_height: Return value for the maximum height of icon images, in pixels.
 *
 * Queries the metrics of items that an icon list item factory is configured to
 * create.
 **/
void
gnome_icon_item_factory_get_item_metrics (GnomeIconItemFactory *factory,
					  int *item_width, int *item_height,
					  int *image_width, int *image_height)
{
	IIFactoryPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory));

	priv = factory->priv;

	if (item_width)
		*item_width = priv->item_width;

	if (item_height)
		*item_height = priv->item_height;

	if (image_width)
		*image_width = priv->image_width;

	if (image_height)
		*image_height = priv->image_height;
}
