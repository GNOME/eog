#include <string.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-macros.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "eog-collection-item.h"
#include "eog-image.h"

struct _EogCollectionItemPrivate {
	EogImage *image;

	gboolean selected;

	GnomeCanvasItem *background;
	GnomeCanvasItem *frame;
	GnomeCanvasItem *pixbuf_item;
	GnomeCanvasItem *caption_item;
	GnomeCanvasItem *selected_item;
};

#define EOG_COLLECTION_ITEM_THUMB_WIDTH  96
#define EOG_COLLECTION_ITEM_THUMB_HEIGHT 96
#define EOG_COLLECTION_ITEM_CAPTION_PADDING 6

static void eog_collection_item_destroy (GtkObject *object);

GNOME_CLASS_BOILERPLATE (EogCollectionItem, eog_collection_item,
			 GnomeCanvasGroup, GNOME_TYPE_CANVAS_GROUP);

static double
eog_collection_item_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
			   GnomeCanvasItem **actual_item)
{
	double x1, y1, x2, y2;
	double width, height;

	*actual_item = item;

	gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);

	width = x2 - x1;
	height = y2 - y1;

	return ((x >= 0) && (y >= 0) && (x <= width) && (y <= height)) ? 0.0 : 1.0;
}

static void
eog_collection_item_class_init (EogCollectionItemClass *klass)
{
        GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

        gobject_class = (GObjectClass *) klass;
	object_class = (GtkObjectClass *) klass;
	item_class = (GnomeCanvasItemClass *) klass;

	item_class->point = eog_collection_item_point;

	object_class->destroy = eog_collection_item_destroy;
}

static void
eog_collection_item_instance_init (EogCollectionItem *item)
{
	EogCollectionItemPrivate *priv;

	priv = g_new0 (EogCollectionItemPrivate, 1);
	priv->image = NULL;
	priv->pixbuf_item = NULL;
	priv->caption_item = NULL;
	priv->selected = FALSE;
	
	item->priv = priv;
}

static void
eog_collection_item_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	EogCollectionItemPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_ITEM (object));

	item = GNOME_CANVAS_ITEM (object);
	priv = EOG_COLLECTION_ITEM (object)->priv;

	/* remember, destroy can be run multiple times! */

	if (priv) {
	    gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	    if (priv->image)
		g_object_unref (priv->image);

	    g_free (priv);
	    EOG_COLLECTION_ITEM (object)->priv = NULL;
	}

	GNOME_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
set_pixbuf (EogCollectionItem *item, GdkPixbuf *pixbuf)
{
	EogCollectionItemPrivate *priv;
	GdkPixbuf *scaled;
	int image_width;
	int image_height;
	int image_x;
	int image_y;

	priv = item->priv;

	image_width = gdk_pixbuf_get_width (pixbuf);
	image_height = gdk_pixbuf_get_height (pixbuf);

	if ((image_width > EOG_COLLECTION_ITEM_THUMB_WIDTH) ||
	    (image_height > EOG_COLLECTION_ITEM_THUMB_HEIGHT))
	{
		int new_width;
		int new_height;
		double factor;

		if (image_width > image_height) {
			factor = (double) EOG_COLLECTION_ITEM_THUMB_WIDTH / (double) image_width;
		}
		else {
			factor = (double) EOG_COLLECTION_ITEM_THUMB_HEIGHT / (double) image_height;
		}

		new_width = image_width * factor;
		new_height = image_height * factor;

		scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf, new_width, new_height);

		image_width = gdk_pixbuf_get_width (scaled);
		image_height = gdk_pixbuf_get_height (scaled);
	}
	else {
		scaled = pixbuf;
		gdk_pixbuf_ref (scaled);
	}

	g_assert (image_width <= EOG_COLLECTION_ITEM_THUMB_WIDTH);
	g_assert (image_height <= EOG_COLLECTION_ITEM_THUMB_HEIGHT);

	image_x = (EOG_COLLECTION_ITEM_THUMB_WIDTH - image_width) / 2;
	image_y = EOG_COLLECTION_ITEM_THUMB_HEIGHT - image_height;

	gnome_canvas_item_set (priv->pixbuf_item, 
			       "pixbuf", scaled, 
			       "x", (double) image_x,
			       "y", (double) image_y,
			       NULL);
	
	gnome_canvas_item_set (priv->frame,
			       "x1", (double) (image_x - 1),
			       "y1", (double) (image_y - 1),
			       "x2", (double) (image_x + image_width - 1),
			       "y2", (double) (image_y + image_height - 1),
			       NULL);
	gnome_canvas_item_show (priv->frame);


	gdk_pixbuf_unref (scaled);
}

static GdkPixbuf*
get_busy_pixbuf (void)
{
	static GdkPixbuf *busy = NULL;

	if (busy == NULL) {
		char *path;
		path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						  "apple-green.png", TRUE, NULL);
		
		busy = gdk_pixbuf_new_from_file (path, NULL);
		g_free (path);
	}

	gdk_pixbuf_ref (busy);

	return busy;
}

static GdkPixbuf*
get_failed_pixbuf (void)
{
	static GdkPixbuf *failed = NULL;

	if (failed == NULL) {
		char *path;
		path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, 
						  "gnome-eog.png", TRUE, NULL);
		
		failed = gdk_pixbuf_new_from_file (path, NULL);
		g_free (path);
	}

	gdk_pixbuf_ref (failed);

	return failed;
}

static GdkBitmap*
get_stipple_bitmap (void)
{
	static char stipple_bits[] = { 0x00, 0x01, 0x01, 0x00 };
	static GdkBitmap *stipple = NULL;

	if (stipple == NULL) {
		stipple = gdk_bitmap_create_from_data (NULL, stipple_bits, 2, 2);
	}

	return stipple;
}

static void
thumbnail_finished_cb (EogImage *image, gpointer data)
{
	EogCollectionItemPrivate *priv;
	GdkPixbuf *pixbuf;

	priv = EOG_COLLECTION_ITEM (data)->priv;

	pixbuf = eog_image_get_pixbuf_thumbnail (priv->image);

	set_pixbuf (EOG_COLLECTION_ITEM (data), pixbuf);

	gdk_pixbuf_unref (pixbuf);
}

static void
thumbnail_failed_cb (EogImage *image, gpointer data)
{
	GdkPixbuf *pixbuf;

	pixbuf = get_failed_pixbuf ();

	g_print ("pixmap failed.%s", eog_image_get_caption (image));

	set_pixbuf (EOG_COLLECTION_ITEM (data), pixbuf);

	gdk_pixbuf_unref (pixbuf);
}	

static void
image_changed_cb (EogImage *image, gpointer data)
{
	EogCollectionItemPrivate *priv;
	GdkPixbuf *pixbuf;

	priv = EOG_COLLECTION_ITEM (data)->priv;

	pixbuf = eog_image_get_pixbuf_thumbnail (priv->image);

	set_pixbuf (EOG_COLLECTION_ITEM (data), pixbuf);

	gdk_pixbuf_unref (pixbuf);
}

/* Shrink the string until its pixel width is <= max_width */
static char*
ensure_max_string_width (gchar *str, PangoLayout *layout, int max_width)
{
	int str_len;
	int str_bytes;
	gchar* str_pt;
	int px_width, px_height;
	int i;
	
	str_len = g_utf8_strlen (str, -1);
	str_bytes = strlen (str);
	
	pango_layout_set_text (layout, str, str_bytes);
	pango_layout_get_pixel_size (layout, &px_width, &px_height);
	
	if (px_width <= max_width) {
		return g_strdup (str);
	}
	
	for (i = 0; i < str_len; i++) {
		str_pt = g_utf8_offset_to_pointer (str, i);
		pango_layout_set_text (layout, str, (int) (str_pt - str));
		pango_layout_get_pixel_size (layout, &px_width, &px_height);
		if (px_width > max_width) {
			i = i--;
			break;
		}
	}
	
	if (i >= str_len - 1) {
		return g_strdup (str);
	}
	else if (i < 0) {
		return NULL;
	}
	else {
		char *result;
		char *tail;
		char *rest;

		tail = g_utf8_offset_to_pointer (str, i+1);
		rest = ensure_max_string_width (tail, layout, max_width);

		result = g_strconcat (g_strndup (str, (tail - str)), "\n", rest, NULL);
		g_free (rest);

		return result;
	}

	return NULL;
}


/* Transforms the string into UTF-8 and inserts line breaks so that no substring is wider than 
 * EOG_COLLECTION_ITEM_THUMB_WIDTH.
 */
static char*
transform_caption (gchar *str, PangoLayout *layout)
{
	char *result;
	char *utf8_str;
	
	if (!g_utf8_validate (str, -9, NULL)) {
		utf8_str = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
	}
	else {
		utf8_str = g_strdup (str);
	}

	result = ensure_max_string_width (utf8_str, layout, EOG_COLLECTION_ITEM_THUMB_WIDTH);
	g_free (utf8_str);
	
	return result;
}

static void
eog_collection_item_construct (EogCollectionItem *item, EogImage *image)
{
	GdkPixbuf *pixbuf;
	EogCollectionItemPrivate *priv;
	GtkStyle *style;
	PangoLayout *layout;
	char *basic_caption;
	char *caption;
	int caption_x;
	int caption_y;
	double x1, y1, x2, y2;

	priv = item->priv;

	priv->image = image;
	g_object_ref (priv->image);

	/* background */
	priv->background = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						  GNOME_TYPE_CANVAS_RECT,
						  "x1", 0.0,
						  "y2", 0.0,
						  "x2", (double) EOG_COLLECTION_ITEM_THUMB_WIDTH,
						  "y2", (double) EOG_COLLECTION_ITEM_THUMB_HEIGHT,
						  "width_pixels", 1,
						  NULL);

	/* Caption */
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas), NULL);
	g_assert (layout != NULL);

	style = GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas)->style;
	g_assert (style != NULL);

	basic_caption = eog_image_get_caption (image);
	caption = transform_caption (basic_caption, layout);

	caption_x = EOG_COLLECTION_ITEM_THUMB_WIDTH / 2;
	caption_y = EOG_COLLECTION_ITEM_THUMB_HEIGHT + 2 + EOG_COLLECTION_ITEM_CAPTION_PADDING;

	priv->caption_item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						    GNOME_TYPE_CANVAS_TEXT,
						    "text", caption,
						    "font_desc", style->font_desc,
						    "anchor", GTK_ANCHOR_NORTH,
						    "justification", GTK_JUSTIFY_CENTER,
						    "x", (double) caption_x,
						    "y", (double) caption_y,
						    "fill_color", style->text[GTK_STATE_NORMAL],
						    NULL);
	g_free (caption);
	     
	/* selection indicator */
	gnome_canvas_item_get_bounds (priv->caption_item, &x1, &y1, &x2, &y2); 
	priv->selected_item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						     GNOME_TYPE_CANVAS_RECT,
						     "x1", (double) (x1 - 2.0),
						     "y1", (double) (y1 - 2.0),
						     "x2", (double) (x2 + 2.0),
						     "y2", (double) (y2 + 2.0),
						     "fill_color", "LightSteelBlue2",
						     "outline_color", "Blue",
						     "width_pixels", 2,
						     "outline_stipple", get_stipple_bitmap (),
						     NULL);
	gnome_canvas_item_lower (priv->selected_item, 1);
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->selected_item));

	/* Image */
	priv->pixbuf_item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						   GNOME_TYPE_CANVAS_PIXBUF,
						   NULL);

	/* Image Frame */
	priv->frame = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
					     GNOME_TYPE_CANVAS_RECT,
					     "width_pixels", 1,
					     "outline_color", "Black",
					     NULL);
	gnome_canvas_item_hide (priv->frame);

	g_signal_connect (image, "thumbnail_failed", G_CALLBACK (thumbnail_failed_cb), item);
	g_signal_connect (image, "thumbnail_finished", G_CALLBACK (thumbnail_finished_cb), item);
	g_signal_connect (image, "image_changed", G_CALLBACK (image_changed_cb), item);

	pixbuf = get_busy_pixbuf ();
	set_pixbuf (item, pixbuf);
	gdk_pixbuf_unref (pixbuf);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (item));
}

GnomeCanvasItem*
eog_collection_item_new (GnomeCanvasGroup *group, EogImage *image)
{
	EogCollectionItem *item;

	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (group), NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	
	item = g_object_new (EOG_TYPE_COLLECTION_ITEM, "parent", group, NULL);

	eog_collection_item_construct (item, image);

	return GNOME_CANVAS_ITEM (item);
}

gboolean 
eog_collection_item_is_selected (EogCollectionItem *item)
{
	g_return_val_if_fail (EOG_IS_COLLECTION_ITEM (item), FALSE);

	return item->priv->selected;
}

void 
eog_collection_item_set_selected (EogCollectionItem *item, gboolean state)
{
	EogCollectionItemPrivate *priv;

	g_return_if_fail (EOG_IS_COLLECTION_ITEM (item));

	priv = item->priv;

	priv->selected = state;
	
	if (priv->selected) 
		gnome_canvas_item_show (priv->selected_item);
	else
		gnome_canvas_item_hide (priv->selected_item);
}

void 
eog_collection_item_toggle_selected (EogCollectionItem *item)
{
	eog_collection_item_set_selected (item, !eog_collection_item_is_selected (item));
}

void
eog_collection_item_load (EogCollectionItem *item)
{
	EogCollectionItemPrivate *priv;

	g_return_if_fail (EOG_IS_COLLECTION_ITEM (item));

	priv = item->priv;

	if (priv->image != NULL && eog_image_load_thumbnail (priv->image)) {
		thumbnail_finished_cb (priv->image, item);
	}
}

EogImage* 
eog_collection_item_get_image (EogCollectionItem *item)
{
	EogCollectionItemPrivate *priv;

	g_return_val_if_fail (EOG_IS_COLLECTION_ITEM (item), NULL);

	priv = item->priv;

	if (priv->image != NULL)
		g_object_ref (priv->image);

	return priv->image;
}

void 
eog_collection_item_get_size (EogCollectionItem *item, int *width, int *height)
{
	int caption_height;
	double x1, x2, y1, y2;

	gnome_canvas_item_get_bounds (item->priv->selected_item, &x1, &y1, &x2, &y2); 

	caption_height = y2 - y1;

	*height = 
		EOG_COLLECTION_ITEM_THUMB_HEIGHT + 
		EOG_COLLECTION_ITEM_CAPTION_PADDING + 
		caption_height;

	*width = EOG_COLLECTION_ITEM_THUMB_WIDTH + 4;
}
