/* Eye of Gnome - default item factory for icons
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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
#include "eog-item-factory-clean.h"
#include "eog-collection-model.h"
#include "cimage.h"
#include <math.h>


#define MAX_CAPTION_LINES 2

/* Private part of the EogItemFactoryClean structure */
struct _EogItemFactoryCleanPrivate {
	/* item metrics */
	EogCleanMetrics *metrics;

	/* default pixmap if the image is not loaded so far */
	GdkPixbuf *dummy;

	/* selection stiple */
	GdkBitmap *stipple;
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
	GnomeCanvasItem *caption_line[MAX_CAPTION_LINES];

	GnomeCanvasItem *cap_rect;

	GnomeCanvasItem *bgr;
} IconItem;



static void eog_item_factory_clean_class_init (EogItemFactoryCleanClass *class);
static void eog_item_factory_clean_instance_init (EogItemFactoryClean *factory);
static void eog_item_factory_clean_dispose (GObject *object);
static void eog_item_factory_clean_finalize (GObject *object);
static EogItemFactoryClean* eog_item_factory_clean_construct (EogItemFactoryClean *factory);

static GnomeCanvasItem *ii_factory_create_item (EogItemFactory *factory,
						GnomeCanvasGroup *parent, guint id);
static void ii_factory_update_item (EogItemFactory *factory,
				    EogCollectionModel *model, 
				    GnomeCanvasItem *item,
				    EogItemUpdateHint hint);
static void ii_factory_get_item_size (EogItemFactory *factory,
				      gint *width, gint *height);

GNOME_CLASS_BOILERPLATE (EogItemFactoryClean, eog_item_factory_clean,
			 EogItemFactory, EOG_TYPE_ITEM_FACTORY);

/* Class initialization function for the icon list item factory */
static void
eog_item_factory_clean_class_init (EogItemFactoryCleanClass *class)
{
	GObjectClass *object_class;
        EogItemFactoryClass *ei_factory_class;

	object_class = (GObjectClass *) class;
	ei_factory_class = (EogItemFactoryClass *) class;

	object_class->dispose = eog_item_factory_clean_dispose;
	object_class->finalize = eog_item_factory_clean_finalize;

	ei_factory_class->create_item = ii_factory_create_item;
	ei_factory_class->update_item = ii_factory_update_item;
	ei_factory_class->get_item_size = ii_factory_get_item_size;
}

/* Object initialization function for the icon list item factory */
static void
eog_item_factory_clean_instance_init (EogItemFactoryClean *factory)
{
	EogItemFactoryCleanPrivate *priv;
	EogCleanMetrics *metrics;
	char stipple_bits[] = { 0x00, 0x01, 0x01, 0x00 };

	priv = g_new0 (EogItemFactoryCleanPrivate, 1);

	metrics = g_new0 (EogCleanMetrics, 1);
	metrics->twidth = 92;
	metrics->theight = 69;
	metrics->cspace = 5;
	metrics->cpadding = 2;
	metrics->font_height = 12;
	priv->metrics = metrics;

	priv->stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);      

	factory->priv = priv;
}

/* Destroy handler for the icon list item factory */
static void
eog_item_factory_clean_dispose (GObject *object)
{
	EogItemFactoryClean *factory;
	EogItemFactoryCleanPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (object));

	factory = EOG_ITEM_FACTORY_CLEAN (object);
	priv = factory->priv;
	
	if (priv->dummy)
		g_object_unref (priv->dummy);
	priv->dummy = NULL;
	
	if (priv->metrics)
		g_free (priv->metrics);
	priv->metrics = NULL;

	if (priv->stipple) 
		g_object_unref (G_OBJECT (priv->stipple));
	priv->stipple = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_item_factory_clean_finalize (GObject *object)
{
	EogItemFactoryClean *factory;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (object));

	factory = EOG_ITEM_FACTORY_CLEAN (object);
	g_free (factory->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object)); 
}



static GtkStyle*
get_style_from_item (GnomeCanvasItem *item)
{
	return gtk_widget_get_style (GTK_WIDGET (item->canvas));
}


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
	int width, height;
	int i;
	GtkStyle *style;

	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (factory), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	eog_item_factory_get_item_size (EOG_ITEM_FACTORY (factory), &width, &height);

	icon = g_new (IconItem, 1);

	icon->id = unique_id;
	icon->item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (parent),
					    GNOME_TYPE_CANVAS_GROUP,
					    NULL);

	style = get_style_from_item (icon->item);
	g_assert (style != NULL);

	icon->bgr = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					   GNOME_TYPE_CANVAS_RECT,
					   "x1", 0.0,
					   "y1", 0.0,
					   "x2", (double) width,
					   "y2", (double) height,
					   "fill_color_gdk", &style->bg[GTK_STATE_NORMAL],
					   NULL);

	for (i = 0; i < MAX_CAPTION_LINES; i++) icon->caption_line[i] = NULL;
	icon->cap_rect = NULL; 
	icon->image = NULL;

	g_object_set_data (G_OBJECT (icon->item), "IconItem", icon);
	g_signal_connect (G_OBJECT (icon->item), "destroy",
			  G_CALLBACK (icon_destroyed),
			  icon);

	return icon->item;
}

/***********************************************************
 *    
 *       Caption Helper Functions
 *
 */

/* Shrink the string until its pixel width is <= max_width */
static char*
ensure_max_string_width (gchar *str, PangoLayout *layout, int max_width, gchar **tail)
{
	char *result;
	int len; 
	int str_len;
	int str_bytes;
	gchar* str_pt;
	int px_width, px_height;
	
	if (tail != NULL) *tail = NULL;

	str_len = g_utf8_strlen (str, -1);
	str_bytes = strlen (str);
	
	pango_layout_set_text (layout, str, str_bytes);
	pango_layout_get_pixel_size (layout, &px_width, &px_height);
	
	if (px_width <= max_width) {
		return g_strdup (str);
	}
	
	len = str_len;
	g_assert (len > 0);
	
	while (px_width > max_width) {
		len--;
		
		if (len <= 0) 
			break;
		
		str_pt = g_utf8_offset_to_pointer (str, len);

		pango_layout_set_text (layout, str, (int) (str_pt - str));
		pango_layout_get_pixel_size (layout, &px_width, &px_height);
	}
	
	if (len > 0) {
		result = g_new0 (gchar, 2 * len);
		result = g_utf8_strncpy (result, str, len);
		if (tail != NULL) {
			*tail = g_strdup (str_pt);
		}
	}
	else {
		result = NULL;
		if (tail != NULL) {
			*tail = g_strdup (str);
		}
	}			
	
	return result;
}

static void
create_item_caption_lines (gchar *str, PangoLayout *layout, int max_width, 
			   char **line, int n_lines)
{
	char *remaining;
	const char *dots = "...";
	int dot_width;
	int px_width, px_height;
	gchar *tail;
	int l;
	
	g_return_if_fail (n_lines > 1);
	g_return_if_fail (str != NULL);
	
	for (l = 0; l < n_lines; l++) {
		line[l] = NULL;
	}
	
	remaining = g_strdup (str);

	for (l = 0; l < n_lines; l++) 
	{
		tail = NULL;

		pango_layout_set_text (layout, remaining, -1);
		pango_layout_get_pixel_size (layout, &px_width, &px_height);
		
		if (px_width <= max_width) {
			line[l] = remaining;
			remaining = NULL;
			break;
		}
		else {
			line[l] = ensure_max_string_width (remaining, layout, max_width, &tail);
			g_free (remaining);
			remaining = NULL;
		}
		
		if (tail == NULL) 
			break;
		else {
			remaining = tail;
		}
	}

	if (remaining != NULL) 
		g_free (remaining);
}

static void
set_caption_text (GnomeCanvasItem *item, 
		  char *caption, 
		  int line,
		  EogItemFactory *factory, 
		  PangoLayout *layout)
{
 	GtkStyle *style;
	IconItem *icon;
	EogCleanMetrics *metrics;
	int caption_w, caption_h;
	int caption_x, caption_y;
	int item_w, item_h;

	g_return_if_fail (item != NULL);

	if (caption == NULL) return;

	icon = g_object_get_data (G_OBJECT (item), "IconItem");
	g_assert (icon != NULL);
	g_return_if_fail (icon->caption_line[line] == NULL);

	metrics = EOG_ITEM_FACTORY_CLEAN (factory)->priv->metrics;

	pango_layout_set_text (layout, caption, -1);
	pango_layout_get_pixel_size (layout, &caption_w, &caption_h);

	eog_item_factory_get_item_size (EOG_ITEM_FACTORY (factory), &item_w, &item_h);

	caption_x = (metrics->twidth - caption_w) / 2;
	caption_y = (metrics->theight + metrics->cspace + metrics->cpadding) 
		+ line * (caption_h + 4);

	style = get_style_from_item (item);
	g_assert (style != NULL);

	icon->caption_line[line] = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
							  GNOME_TYPE_CANVAS_TEXT,
							  "text", caption,
							  "font_desc", style->font_desc,
							  "anchor", GTK_ANCHOR_NW,
							  "x", (double) caption_x,
							  "y", (double) caption_y,
							  "fill_color", style->text[GTK_STATE_NORMAL],
							  NULL);
}

/***********************************************************
 *    
 *       Update Functions
 *
 */

static void
update_item_image (EogItemFactoryClean *factory, GnomeCanvasItem *item, CImage *cimage)
{
	EogItemFactoryCleanPrivate *priv;
	EogCleanMetrics *metrics;
	IconItem *icon;
	int image_w, image_h;
	int image_x, image_y;
	double x1, y1, x2, y2;
	GdkPixbuf *thumb;
	
	metrics = factory->priv->metrics;
	priv = factory->priv;

	icon = g_object_get_data (G_OBJECT (item), "IconItem");
	g_assert (icon != NULL);

	/* obtain thumbnail image */
	if (cimage_has_thumbnail (cimage)) {
		thumb = cimage_get_thumbnail (cimage);
	} 
	else {
		thumb = priv->dummy;
		g_object_ref (thumb);
	}
	g_assert (thumb != NULL);

	if (icon->image != NULL) {
		gtk_object_destroy (GTK_OBJECT (icon->image));
	}

	image_w = gdk_pixbuf_get_width (thumb);
	image_h = gdk_pixbuf_get_height (thumb);
	image_x = (metrics->twidth - image_w) / 2;
	image_y = (metrics->theight - image_h) / 2;

	icon->image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
					     GNOME_TYPE_CANVAS_PIXBUF,
					     "pixbuf", thumb,
					     "x", (double) image_x,
					     "y", (double) image_y,
					     "width_set", FALSE,
					     "height_set", FALSE,
					     NULL);
	
	g_object_unref (thumb);
}

static void
update_item_caption (EogItemFactoryClean *factory, GnomeCanvasItem *item, CImage *cimage)
{
	PangoLayout  *layout;
	IconItem *icon;
	gchar *caption[MAX_CAPTION_LINES];
	gchar *full_caption;
	EogCleanMetrics *metrics;
	int i;
	double x1, y1, x2, y2;
	double left, right, top, bottom;

	/* obtain some objects */
	metrics = factory->priv->metrics;

	icon = g_object_get_data (G_OBJECT (item), "IconItem");
	g_assert (icon != NULL);

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (item->canvas), NULL);
	g_assert (layout != NULL);
	
	/* obtain caption text */
	if (cimage_has_caption (cimage)) {
		full_caption = cimage_get_caption (cimage);
	}
	else {
		GnomeVFSURI *uri;
		uri = cimage_get_uri (cimage);
		full_caption = g_path_get_basename (gnome_vfs_uri_get_path (uri));
		gnome_vfs_uri_unref (uri);

		if (!g_utf8_validate (full_caption, -1, 0)) {
			gchar *tmp = g_locale_to_utf8 (full_caption, -1, 0, 0, 0);
			g_free (full_caption);
			full_caption = tmp;
		}

		cimage_set_caption (cimage, full_caption);
	}
	g_assert (full_caption != NULL);

	/* spread caption over two text lines, if neccessary */
	for (i = 0; i < MAX_CAPTION_LINES; i++)  caption[i] = NULL;
	create_item_caption_lines (full_caption, layout, 
				   metrics->twidth - 2 * metrics->cpadding, 
				   caption, MAX_CAPTION_LINES);
	
	/* setup canvas text items */
	for (i = 0; i < MAX_CAPTION_LINES; i++) {
		if (icon->caption_line[i] != NULL) {
			gtk_object_destroy (GTK_OBJECT (icon->caption_line[i]));
			icon->caption_line[i] = NULL;
		}

		set_caption_text (item, caption[i], i, EOG_ITEM_FACTORY (factory), layout);
		g_free (caption[i]);
	}

	/* setup text selection item */
	left = top = 10000.0;
	right = bottom = 0;
	
	for (i = 0; i < MAX_CAPTION_LINES; i++) {
		if (icon->caption_line[i] != NULL) {
			gnome_canvas_item_get_bounds (icon->caption_line[i], 
						      &x1, &y1, &x2, &y2);
			left = MIN (left, x1);
			right = MAX (right, x2);
			top = MIN (top, y1);
			bottom = MAX (bottom, y2);
		}
	}
	
	left = left - metrics->cpadding;
	right = right + metrics->cpadding;
	top = top - metrics->cpadding ;
	bottom = bottom + metrics->cpadding;

	if (icon->cap_rect != NULL)
		gtk_object_destroy (GTK_OBJECT (icon->cap_rect));
	
	icon->cap_rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						GNOME_TYPE_CANVAS_RECT,
						"x1", left,
						"y1", top,
						"x2", right,
						"y2", bottom,
						"fill_color", "LightSteelBlue2",
						"outline_color", "Blue",
						"width_pixels", 2,
						"outline_stipple", factory->priv->stipple,
						NULL);
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (icon->cap_rect));

	for (i = 0; i < MAX_CAPTION_LINES; i++) {
		if (icon->caption_line[i] != NULL)
			gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM (icon->caption_line[i]));
	}

	/* clean up */
	g_object_unref (layout);
	g_free (full_caption);
}	


static void
update_item_selection (EogItemFactoryClean *factory, GnomeCanvasItem *item, CImage *cimage)
{
	IconItem *icon;
	icon = g_object_get_data (G_OBJECT (item), "IconItem");
	g_assert (icon != NULL);

	if (icon->cap_rect == NULL) return;

	if (cimage_is_selected (cimage)) {
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (icon->cap_rect));
	}
	else {
		gnome_canvas_item_hide (GNOME_CANVAS_ITEM (icon->cap_rect));
	}
}

/* Configure_item handler for the icon list item factory */
static void
ii_factory_update_item (EogItemFactory *factory, 
			EogCollectionModel *model,
			GnomeCanvasItem *item,
			EogItemUpdateHint hint)
{
	EogItemFactoryClean *ii_factory;
	GdkPixbuf *thumb;
	EogItemFactoryCleanPrivate *priv;
	EogCleanMetrics *metrics;
	CImage *cimage = NULL;
	IconItem *icon;
	int image_w, image_h;
	int image_x, image_y;
	int caption_w, caption_h;
	int caption_x, caption_y;
	int item_w, item_h;
	gchar *caption;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	ii_factory = EOG_ITEM_FACTORY_CLEAN (factory);
	
	icon = g_object_get_data (G_OBJECT (item), "IconItem");
	cimage = eog_collection_model_get_image (model, icon->id);
	g_return_if_fail (cimage != NULL);

	if ((hint & EOG_ITEM_UPDATE_IMAGE) == EOG_ITEM_UPDATE_IMAGE) {
		update_item_image (ii_factory, item, cimage);
	}

	if ((hint & EOG_ITEM_UPDATE_CAPTION) == EOG_ITEM_UPDATE_CAPTION) {
		update_item_caption (ii_factory, item, cimage);
	}

	if ((hint & EOG_ITEM_UPDATE_SELECTION_STATE) == EOG_ITEM_UPDATE_SELECTION_STATE) {
		update_item_selection (ii_factory, item, cimage);
	}
}


/***********************************************************
 *    
 *       EogItemFactory API functions
 *
 */

/* Get_item_size handler for the icon list item factory */
static void
ii_factory_get_item_size (EogItemFactory *factory,
			  gint *width, gint *height)
{
	EogItemFactoryClean *ii_factory;
	EogCleanMetrics *metrics;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (factory));

	ii_factory = EOG_ITEM_FACTORY_CLEAN (factory);
	metrics = ii_factory->priv->metrics;

	*width  = metrics->twidth;
	*height = metrics->theight      /* height of the image */
		+ 2 * metrics->cpadding /* padding around the caption */
		+ metrics->cspace       /* space between caption and image */
		+ 2 * metrics->font_height       /* caption height */
		+ 4;                    /* padding between caption lines */ 
}

static EogItemFactoryClean*
eog_item_factory_clean_construct (EogItemFactoryClean *factory)
{
	EogItemFactoryCleanPrivate *priv;
	char *dummy_file;
	guchar *buffer;

	priv = factory->priv;

	/* load dummy pixmap file */
	dummy_file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						"gnome-eog.png", TRUE, NULL);
	priv->dummy = gdk_pixbuf_new_from_file (dummy_file, NULL);
	g_free (dummy_file);

	return factory;
}


EogItemFactoryClean *
eog_item_factory_clean_new (void)
{
	EogItemFactoryClean *factory;

	factory = EOG_ITEM_FACTORY_CLEAN (g_object_new (EOG_TYPE_ITEM_FACTORY_CLEAN, NULL));
	return eog_item_factory_clean_construct (factory);
}

/**
 * eog_item_factory_clean_set_item_metrics:
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
eog_item_factory_clean_set_metrics (EogItemFactoryClean *factory,
				     EogCleanMetrics *metrics)
{
	EogItemFactoryCleanPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (factory));

	priv = factory->priv;

	if (priv->metrics)
		g_free (priv->metrics);
	
	priv->metrics = metrics;

	eog_item_factory_configuration_changed (EOG_ITEM_FACTORY (factory));
}

EogCleanMetrics* 
eog_item_factory_clean_get_metrics (EogItemFactoryClean *factory)
{
	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (EOG_IS_ITEM_FACTORY_CLEAN (factory), NULL);

	return factory->priv->metrics;
}
