#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkicontheme.h>

#include "eog-collection-item.h"
#include "eog-image.h"
#include "eog-canvas-pixbuf.h"
#include "eog-job-manager.h"
#include "eog-thumbnail.h"

struct _EogCollectionItemPrivate {
	EogImage *image;

	gboolean selected;
	gboolean emit_changed_signal;

	GnomeCanvasItem *frame;
	GnomeCanvasItem *pixbuf_item;
	GnomeCanvasItem *caption_item;
	GnomeCanvasItem *selected_item;
};


enum {
	SIGNAL_SIZE_CHANGED,
	SIGNAL_LAST
};
static gint eog_collection_item_signals [SIGNAL_LAST];

static void eog_collection_item_destroy (GtkObject *object);
static void eog_collection_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static char* get_item_image_caption (GnomeCanvasItem *item, EogImage *image);
static char* ensure_max_string_width (gchar *str, PangoLayout *layout, int max_width);


G_DEFINE_TYPE (EogCollectionItem, eog_collection_item, GNOME_TYPE_CANVAS_GROUP)

static void
eog_collection_item_class_init (EogCollectionItemClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) klass;
	item_class = (GnomeCanvasItemClass *) klass;

	object_class->destroy = eog_collection_item_destroy;
	item_class->update = eog_collection_item_update;

	eog_collection_item_signals [SIGNAL_SIZE_CHANGED] = 
		g_signal_new ("size_changed",
			      G_TYPE_OBJECT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogCollectionItemClass, size_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);			     
}

static void
eog_collection_item_init (EogCollectionItem *item)
{
	EogCollectionItemPrivate *priv;

	priv = g_new0 (EogCollectionItemPrivate, 1);
	priv->image = NULL;
	priv->pixbuf_item = NULL;
	priv->caption_item = NULL;
	priv->selected = FALSE;
	priv->emit_changed_signal = FALSE;
	
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

	    if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	    }

	    g_free (priv);
	    EOG_COLLECTION_ITEM (object)->priv = NULL;
	}

	GTK_OBJECT_CLASS (eog_collection_item_parent_class)->destroy (object);
}

static void
eog_collection_item_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	EogCollectionItemPrivate *priv;

	priv = EOG_COLLECTION_ITEM (item)->priv;

	GNOME_CANVAS_ITEM_CLASS (eog_collection_item_parent_class)->update (
									item,
									affine,
									clip_path,
									flags);

	if (priv->emit_changed_signal) {
		priv->emit_changed_signal = FALSE;
		g_signal_emit (G_OBJECT (item), eog_collection_item_signals [SIGNAL_SIZE_CHANGED], 0);
	}
}

static void
set_pixbuf (EogCollectionItem *item, GdkPixbuf *pixbuf, gboolean view_frame)
{
	EogCollectionItemPrivate *priv;
	GdkPixbuf *scaled;
	GdkPixbuf *old_pixbuf;
	int old_width = 0;
	int old_height = 0;
	int image_width;
	int image_height;
	int image_x;
	int image_y;

	priv = item->priv;

	/* determine size of the old pixbuf */
	g_object_get (G_OBJECT (priv->pixbuf_item), "pixbuf", &old_pixbuf, NULL);
	if (old_pixbuf != NULL) {
		old_width = gdk_pixbuf_get_width (old_pixbuf);
		old_height = gdk_pixbuf_get_height (old_pixbuf);
		g_object_unref (old_pixbuf);
	}

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

		new_height = MAX (new_height, 1);
		new_width = MAX (new_width, 1);

		scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf, new_width, new_height);

		image_width = gdk_pixbuf_get_width (scaled);
		image_height = gdk_pixbuf_get_height (scaled);
	}
	else {
		scaled = pixbuf;
		g_object_ref (scaled);
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
			       "x1", (double) (image_x - EOG_COLLECTION_ITEM_FRAME_WIDTH),
			       "y1", (double) (image_y - EOG_COLLECTION_ITEM_FRAME_WIDTH),
			       "x2", (double) (image_x + image_width - 1),
			       "y2", (double) (image_y + image_height - 1),
			       NULL);
	if (view_frame) {
		gnome_canvas_item_show (priv->frame);
	}
	else {
		gnome_canvas_item_hide (priv->frame);
	}

	g_object_unref (scaled);

	/* emit size changed signal if pixbuf size changed */
	if (image_width != old_width || image_height != old_height) {
		priv->emit_changed_signal = TRUE;
	}
}

static GdkPixbuf*
get_busy_pixbuf (void)
{
	static GdkPixbuf *busy = NULL;

	if (busy == NULL) {
		GtkIconTheme *icon_theme;
		GError *error = NULL;

		icon_theme = gtk_icon_theme_get_default ();
		busy = gtk_icon_theme_load_icon (icon_theme,
						 "image-loading", /* icon name */
						 EOG_COLLECTION_ITEM_THUMB_WIDTH, /* size */
						   0,  /* flags */
						   &error);
		if (!busy) {
			g_warning ("Couldn't load icon: %s", error->message);
			g_error_free (error);
		}
	}

	g_object_ref (busy);

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

	g_object_ref (failed);

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
job_thumb_finished (EogJob *job, gpointer data, GError *error)
{
	EogCollectionItemPrivate *priv;
	GdkPixbuf *pixbuf = NULL;
	gboolean show_frame = FALSE;
	
	priv = EOG_COLLECTION_ITEM (data)->priv;

	if (!priv)
		return;
		
	if (eog_job_get_success (job)) {
		/* success */
		pixbuf = eog_image_get_pixbuf_thumbnail (priv->image);
		show_frame = TRUE;
	}
	
	if (pixbuf == NULL) {
		pixbuf = get_failed_pixbuf ();
	}
	
	g_assert (pixbuf != NULL);
	
	set_pixbuf (EOG_COLLECTION_ITEM (data), pixbuf, show_frame);
	g_object_unref (pixbuf);
}

static void 
job_thumb_create (EogJob *job, gpointer data, GError **error)
{
	EogCollectionItemPrivate *priv;
	EogImage *image;
	GdkPixbuf *pixbuf;
	GnomeVFSURI *uri;

	priv = EOG_COLLECTION_ITEM (data)->priv;

	image = priv->image; 
	if (!EOG_IS_IMAGE (image))
		return;

	uri = eog_image_get_uri (image);
	if (uri == NULL)
		return;
		
	pixbuf = eog_thumbnail_load (uri, job, error);

	if (pixbuf == NULL) 
		g_assert (*error != NULL);
	else {
		eog_image_set_thumbnail (image, pixbuf);
		g_object_unref (pixbuf);
	}
	
	gnome_vfs_uri_unref (uri);
}

static void
image_changed_cb (EogImage *image, gpointer data)
{
	EogCollectionItemPrivate *priv;
	GdkPixbuf *pixbuf;
	char *caption;
	double x1, x2, y1, y2;

	priv = EOG_COLLECTION_ITEM (data)->priv;

	/* update thumnbail */
	pixbuf = eog_image_get_pixbuf_thumbnail (priv->image);
	set_pixbuf (EOG_COLLECTION_ITEM (data), pixbuf, TRUE);
	g_object_unref (pixbuf);

	/* update caption */
	caption = get_item_image_caption (GNOME_CANVAS_ITEM (data), image);
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->caption_item),
			       "markup", caption,
			       NULL);
	g_free (caption);

	/* update caption frame size */
	gnome_canvas_item_get_bounds (priv->caption_item, &x1, &y1, &x2, &y2); 
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->selected_item),
			       "x1", (double) (x1 - EOG_COLLECTION_ITEM_CAPTION_PADDING),
			       "y1", (double) (y1 - EOG_COLLECTION_ITEM_CAPTION_PADDING),
			       "x2", (double) (x2 + EOG_COLLECTION_ITEM_CAPTION_PADDING),
			       "y2", (double) (y2 + EOG_COLLECTION_ITEM_CAPTION_PADDING),
			       NULL);
}

static char*
get_item_image_caption (GnomeCanvasItem *item, EogImage *image)
{
	PangoLayout *layout;
	char *basic_caption;
	char *caption;
	char *unescaped;

	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (item), NULL);

	basic_caption = eog_image_get_caption (image); /* don't free basic_caption */
	if (basic_caption == NULL) return NULL;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (item->canvas), NULL);
	g_assert (layout != NULL);

	/* add line breaks */
	unescaped = ensure_max_string_width (basic_caption, layout, EOG_COLLECTION_ITEM_THUMB_WIDTH);
	caption = g_markup_escape_text (unescaped, strlen (unescaped));
	g_free (unescaped);
	
	/* set bold caption to indicate image modification */
	if (eog_image_is_modified (image)) {
		char *marked;
		
		marked = g_strdup_printf("<b>%s</b>", caption);

		g_free (caption);
		caption = marked;
	}

	g_object_unref (layout);
	
	return caption;
}

/* Shrink the string until its pixel width is <= max_width */
static char*
ensure_max_string_width (gchar *str, PangoLayout *layout, int max_width)
{
	int str_len;
	int str_bytes;
	int split_point = -1;
	gchar* str_pt;
	int px_width, px_height;
	int i;
	gunichar uc;
	char *result = NULL;

	str_len = g_utf8_strlen (str, -1);
	str_bytes = strlen (str);
	
	pango_layout_set_text (layout, str, str_bytes);
	pango_layout_get_pixel_size (layout, &px_width, &px_height);
	
	/* return the whole string if it's smaller than max_width */
	if (px_width <= max_width) {
		return g_strdup (str);
	}
	
	/* The string is larger than max_width.  Now, try to find a
	 * position in the string to split it into two parts, where
	 * the first string mustn't be wider as max_width. The rest of
	 * the string will be splitted by a recursive call to this
	 * function then.
	 */

	/* check until which character position the string is not
	 * larger than max_width */
	for (i = 0; i < str_len; i++) {
		str_pt = g_utf8_offset_to_pointer (str, i);

		pango_layout_set_text (layout, str, (int) (str_pt - str));
		pango_layout_get_pixel_size (layout, &px_width, &px_height);

		if (px_width > max_width) {
			/* set i to position to split */
			if (split_point >= 0) {
				i = split_point;
			}
			else {
				--i;
			}
			break;
		}

		/* Store last sensible split position. This is any non
		 * alphanumeric character (eg. dot, dash). If we can
		 * split a string there it looks and reads much
		 * better.
		 */
		uc = g_utf8_get_char (str_pt); 
		if (!g_unichar_isalnum (uc)) { 
			split_point = i; 
		} 
	}
	
	/* Variable i now points to the last character which may stay
	 * in the first half of the string */

	if (i >= str_len - 1) {
		result = g_strdup (str);
	}
	else if (i < 0) {
		result = NULL;
	}
	else {
		char *tail;
		char *rest;
		char *tmp;

		/* split remaining rest of string */
		tail = g_utf8_offset_to_pointer (str, i+1);
		rest = ensure_max_string_width (tail, layout, max_width);
		tmp = g_strndup (str, (tail - str));

		result = g_strconcat ((tmp), "\n", rest, NULL);
		g_free (tmp);
		g_free (rest);
	}

	return result;
}

static void
eog_collection_item_construct (EogCollectionItem *item, EogImage *image)
{
	GdkPixbuf *pixbuf;
	EogCollectionItemPrivate *priv;
	GtkStyle *style;
	char *caption;
	int caption_x;
	int caption_y;
	double x1, y1, x2, y2;

	priv = item->priv;

	priv->image = image;
	g_object_ref (priv->image);

	/* Caption */
	style = GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas)->style;
	g_assert (style != NULL);

	caption = get_item_image_caption (GNOME_CANVAS_ITEM (item), image);

	caption_x = EOG_COLLECTION_ITEM_THUMB_WIDTH / 2;
	caption_y = EOG_COLLECTION_ITEM_THUMB_HEIGHT + 2 * EOG_COLLECTION_ITEM_FRAME_WIDTH + EOG_COLLECTION_ITEM_SPACING;

	priv->caption_item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						    GNOME_TYPE_CANVAS_TEXT,
						    "markup", caption,
						    "font_desc", style->font_desc,
						    "anchor", GTK_ANCHOR_NORTH,
						    "justification", GTK_JUSTIFY_CENTER,
						    "x", (double) caption_x,
						    "y", (double) caption_y,
						    "fill_color_gdk", &style->text[GTK_STATE_NORMAL],
						    NULL);
	g_free (caption);
	     
	/* selection indicator */
	gnome_canvas_item_get_bounds (priv->caption_item, &x1, &y1, &x2, &y2); 
	priv->selected_item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						     GNOME_TYPE_CANVAS_RECT,
						     "x1", (double) (x1 - EOG_COLLECTION_ITEM_CAPTION_PADDING),
						     "y1", (double) (y1 - EOG_COLLECTION_ITEM_CAPTION_PADDING),
						     "x2", (double) (x2 + EOG_COLLECTION_ITEM_CAPTION_PADDING),
						     "y2", (double) (y2 + EOG_COLLECTION_ITEM_CAPTION_PADDING),
						     "fill_color", "LightSteelBlue2",
						     "outline_color", "Blue",
						     "width_pixels",  EOG_COLLECTION_ITEM_CAPTION_FRAME_WIDTH,
						     "outline_stipple", get_stipple_bitmap (),
						     NULL);
	gnome_canvas_item_lower (priv->selected_item, 1);
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->selected_item));

	/* Image */
	priv->pixbuf_item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
						   EOG_TYPE_CANVAS_PIXBUF,
						   NULL);

	/* Image Frame */
	priv->frame = gnome_canvas_item_new (GNOME_CANVAS_GROUP (item),
					     GNOME_TYPE_CANVAS_RECT,
					     "width_pixels", EOG_COLLECTION_ITEM_FRAME_WIDTH,
					     "outline_color", "Black",
					     NULL);
	gnome_canvas_item_hide (priv->frame);

	g_signal_connect (image, "image_changed", G_CALLBACK (image_changed_cb), item);

	pixbuf = get_busy_pixbuf ();
	set_pixbuf (item, pixbuf, FALSE);
	g_object_unref (pixbuf);

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
	EogJob *job;
	GdkPixbuf *thumb = NULL;

	g_return_if_fail (EOG_IS_COLLECTION_ITEM (item));

	priv = item->priv;

	if (priv->image == NULL) 
		return;

	thumb = eog_image_get_pixbuf_thumbnail (priv->image);
	if (thumb == NULL) {
		/* try to create thumbnail */
		job = eog_job_new (G_OBJECT (item),
				   job_thumb_create,
				   job_thumb_finished,
				   NULL, /* cancel */
				   NULL); /* progress */
		eog_job_manager_add (job);
		g_object_unref (G_OBJECT (job));
	}
	else {
		set_pixbuf (EOG_COLLECTION_ITEM (item), thumb, TRUE);
		g_object_unref (thumb);
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

/* eog_collection_item_get_size:
 *
 * Returns the width and the height of the image and the caption seperately. This is
 * used by EogWrapList to align the items at the image bottom
 * line. caption_height includes the padding between image and caption
 * so that image_height + caption_height gives the total height of the item.
 */
void 
eog_collection_item_get_size (EogCollectionItem *item, int *width, int *image_height, int *caption_height)
{
	double x1, x2, y1, y2;
	int img_width;
	int cap_width;

	/* FIXME: maybe we should cache the values here, because they get
	 * calculated at least 2 times for every item during item-rearrangement of 
	 * the wrap list.
	 */
	gnome_canvas_item_get_bounds (item->priv->selected_item, &x1, &y1, &x2, &y2); 
	*caption_height = (int) EOG_COLLECTION_ITEM_CAPTION_PADDING + y2 - y1;
	cap_width = (int) x2 - x1;
	
	gnome_canvas_item_get_bounds (item->priv->frame, &x1, &y1, &x2, &y2); 
	*image_height = (int) y2 - y1;
	img_width = (int) x2 - x1;

	*width = MAX (cap_width, img_width);
}
