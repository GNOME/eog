#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-preview-page.h>

#define PARENT_TYPE GNOME_TYPE_CANVAS
static GnomeCanvasClass *parent_class = NULL;

struct _EogPreviewPagePrivate
{
	EogImageView	*image_view;

	gint		 col;
	gint		 row;

	GnomeCanvasItem *group;
	GnomeCanvasItem *bg_white;
	GnomeCanvasItem *bg_black;
	GnomeCanvasItem *image;

	gint		 width;
	gint		 height;
};

#define SCALE(param) (0.15 * param)
#define OFFSET_X 2
#define OFFSET_Y 2

static void
resize_paper (EogPreviewPage *page, gint width, gint height)
{
	page->priv->width = width;
	page->priv->height = height;

	gnome_canvas_set_scroll_region (GNOME_CANVAS (page), 0.0, 0.0,
					OFFSET_X + width, OFFSET_Y + height);
	gtk_widget_set_usize (GTK_WIDGET (page), 
			      OFFSET_X + width + 1, OFFSET_Y + height + 1);
	
	if (page->priv->bg_black)
		gtk_object_destroy (GTK_OBJECT (page->priv->bg_black));
	if (page->priv->bg_white)
		gtk_object_destroy (GTK_OBJECT (page->priv->bg_white));
	
	page->priv->bg_black = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (page->priv->group),
		gnome_canvas_rect_get_type (), 
		"x1", 0.0, "y1", 0.0, 
		"x2", (double) page->priv->width, 
		"y2", (double) page->priv->height, 
		"fill_color", "black", 
		"outline_color", "black", 
		"width_pixels", 1, NULL);
	page->priv->bg_white = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (page->priv->group), 
		gnome_canvas_rect_get_type (), 
		"x1", (double) OFFSET_X, 
		"y1", (double) OFFSET_Y, 
		"x2", (double) OFFSET_X + width, 
		"y2", (double) OFFSET_Y + height, 
		"fill_color", "white", 
		"outline_color", "black", 
		"width_pixels", 1, NULL); 
}

void
eog_preview_page_update (EogPreviewPage *page,
			 gint width, gint height, 
			 gint bottom, gint top, gint right, gint left,
			 gint adjust_to, gboolean fit_to_page,
			 gboolean vertically, gboolean horizontally, 
			 gint *cols_needed, gint *rows_needed)
{
	BonoboPropertyBag 	*bag;
	BonoboArg		*arg;
	EogImage		*image;
	GtkWidget		*widget;
	GdkInterpType		 interpolation;
	GdkPixbuf		*pixbuf_orig;
	GdkPixbuf		*pixbuf;
	GdkPixmap		*pixmap;
	GdkBitmap		*bitmap;
	gint			 pixbuf_width, pixbuf_height;
	gint			 image_width, image_height;
	gint			 avail_width, avail_height;
	gint			 leftover_width, leftover_height;
	gint			 x, y;
	gboolean		 first_x, last_x;
	gboolean		 first_y, last_y;

	/* In case anything goes wrong... */
	*cols_needed = 1;
	*rows_needed = 1;

	if ((width != page->priv->width) || (height != page->priv->height))
		resize_paper (page, width, height);
	
	/* How much place do we have got on the paper? */
	avail_width = width - left - right;
	avail_height = height - top - bottom;
	g_return_if_fail (avail_width > 0);
	g_return_if_fail (avail_height > 0);

	/* Get the pixbuf */
	image = eog_image_view_get_image (page->priv->image_view);
	pixbuf_orig = eog_image_get_pixbuf (image);
	bonobo_object_unref (BONOBO_OBJECT (image));
	g_return_if_fail (pixbuf_orig);

	/* Get the interpolation type */
	bag = eog_image_view_get_property_bag (page->priv->image_view);
	arg = bonobo_property_bag_get_value (bag, "interpolation", NULL);
	bonobo_object_unref (BONOBO_OBJECT (bag));
	g_return_if_fail (arg);
	switch (*(GNOME_EOG_Interpolation*)arg->_value) {
	case GNOME_EOG_INTERPOLATION_NEAREST: 
		interpolation = GDK_INTERP_NEAREST; 
		break; 
	case GNOME_EOG_INTERPOLATION_TILES: 
		interpolation = GDK_INTERP_TILES; 
		break; 
	case GNOME_EOG_INTERPOLATION_BILINEAR: 
		interpolation = GDK_INTERP_BILINEAR; 
		break; 
	case GNOME_EOG_INTERPOLATION_HYPERBOLIC: 
		interpolation = GDK_INTERP_HYPER; 
		break; 
	default: 
		g_warning ("Got unknow interpolation type!"); 
		interpolation = GDK_INTERP_NEAREST; 
	} 
	bonobo_arg_release (arg); 

	/* How big is the pixbuf? */
	pixbuf_width = SCALE (gdk_pixbuf_get_width (pixbuf_orig));
	pixbuf_height = SCALE (gdk_pixbuf_get_height (pixbuf_orig));

	/* Shall we fit the image onto one page? */
	if (fit_to_page) {
		double prop_paper, prop_pixbuf;

		*cols_needed = 1;
		*rows_needed = 1;

		if (page->priv->col || page->priv->row) 
			return;

		first_x = TRUE;
		first_y = TRUE;
		last_x = TRUE;
		last_y = TRUE;

		prop_paper = (double) avail_height / avail_width;
		prop_pixbuf = (double) pixbuf_height / pixbuf_width;

		/* Calculate width and height of image */
		if (prop_pixbuf > prop_paper) {
			image_width = avail_height / prop_pixbuf;
			image_height = avail_height;
		} else {
			image_width = avail_width;
			image_height = avail_width * prop_pixbuf;
		}
		g_return_if_fail (image_width > 0);
		g_return_if_fail (image_height > 0);

		leftover_width = avail_width - image_width;
		leftover_height = avail_height - image_height;

		pixbuf = gdk_pixbuf_scale_simple (pixbuf_orig,
						  image_width, image_height,
						  interpolation);
	} else {
		GdkPixbuf *adj_pixbuf;
		gint	   adj_width, adj_height;

		adj_width = pixbuf_width * adjust_to / 100;
		adj_height = pixbuf_height * adjust_to / 100;
		g_return_if_fail (adj_width > 0);
		g_return_if_fail (adj_height > 0);

		/* Calculate the free place on the paper */
		leftover_width = avail_width - (adj_width % avail_width);
		leftover_width = leftover_width % avail_width;
		leftover_height = avail_height - (adj_height % avail_height);
		leftover_height = leftover_height % avail_height;

		adj_pixbuf = gdk_pixbuf_scale_simple (pixbuf_orig, 
						      adj_width, 
						      adj_height, 
						      interpolation); 

		*cols_needed = (adj_width + leftover_width) / avail_width;
		*rows_needed = (adj_height + leftover_height) / avail_height;
		
		if (*cols_needed <= page->priv->col)
			return;
		if (*rows_needed <= page->priv->row)
			return;

		first_x = (page->priv->col == 0);
		first_y = (page->priv->row == 0);
		last_x = (*cols_needed == page->priv->col + 1);
		last_y = (*rows_needed == page->priv->row + 1);
		
		/* Width of image? */
		if (first_x && last_x)
			image_width = adj_width;
		else if (last_x) {
			image_width = ((adj_width - 1) % avail_width) + 1;
			if (horizontally)
				image_width += leftover_width / 2;
		} else {
			image_width = avail_width;
			if (first_x && horizontally)
				image_width -= leftover_width / 2;
		}
		g_return_if_fail (image_width > 0);

		/* Height of image? */
		if (first_y && last_y) 
			image_height = adj_height; 
		else if (last_y) { 
			image_height = ((adj_height - 1) % avail_height) + 1; 
			if (vertically) 
				image_height += leftover_height / 2; 
		} else { 
			image_height = avail_height; 
			if (first_y && vertically) 
				image_height -= leftover_height / 2; 
		} 
		g_return_if_fail (image_height > 0); 
		
		pixbuf = gdk_pixbuf_new (
				gdk_pixbuf_get_colorspace (adj_pixbuf),
				gdk_pixbuf_get_has_alpha (adj_pixbuf), 
				gdk_pixbuf_get_bits_per_sample (adj_pixbuf), 
				image_width, image_height); 
		
		/* Where do we begin to copy (x)? */ 
		if (first_x) 
			x = 0; 
		else if (last_x) 
			x = adj_width - image_width; 
		else { 
			x = avail_width * page->priv->col; 
			if (horizontally) 
				x -= leftover_width / 2; 
		} 
		g_return_if_fail (x >= 0); 
		
		/* Where do we begin to copy (y)? */ 
		if (first_y) 
			y = 0; 
		else if (last_y) 
			y = adj_height - image_height; 
		else { 
			y = avail_height * page->priv->row; 
			if (vertically) 
				y -= leftover_height / 2; 
		} 
		g_return_if_fail (y >= 0); 
		
		gdk_pixbuf_copy_area (adj_pixbuf, x, y, 
				      image_width, image_height, pixbuf, 0, 0);

		gdk_pixbuf_unref (adj_pixbuf);
	}

	gdk_pixbuf_unref (pixbuf_orig);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 1);
	gdk_pixbuf_unref (pixbuf); 
	widget = gtk_pixmap_new (pixmap, bitmap); 
	gtk_widget_show (widget);

	/* Where to put the image (x)? */ 
	x = OFFSET_X + left; 
	if ((horizontally && !fit_to_page && first_x) || 
	    (horizontally && fit_to_page)) 
	    	x += leftover_width / 2; 
	
	/* Where to put the image (y)? */ 
	y = OFFSET_Y + top; 
	if ((vertically && !fit_to_page && first_y) || 
	    (vertically && fit_to_page)) 
	    	y += leftover_height / 2; 
	
	if (page->priv->image) 
		gtk_object_destroy (GTK_OBJECT (page->priv->image)); 
	
	page->priv->image = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (page->priv->group), 
				gnome_canvas_widget_get_type (), 
				"widget", widget, 
				"x", (double) x, 
				"y", (double) y, 
				"width", (double) image_width, 
				"height", (double) image_height, 
				"anchor", GTK_ANCHOR_NORTH_WEST, 
				"size_pixels", TRUE, NULL);
}

static void
eog_preview_page_destroy (GtkObject *object)
{
	EogPreviewPage *page;

	page = EOG_PREVIEW_PAGE (object);

	g_free (page->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
eog_preview_page_init (EogPreviewPage *page)
{
	page->priv = g_new0 (EogPreviewPagePrivate, 1);
}

static void
eog_preview_page_class_init (EogPreviewPageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = eog_preview_page_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
}

GtkType
eog_preview_page_get_type (void)
{
	static GtkType page_type = 0;

	if (!page_type) {
		static const GtkTypeInfo page_info = {
			"EogPreviewPage",
			sizeof (EogPreviewPage),
			sizeof (EogPreviewPageClass),
			(GtkClassInitFunc) eog_preview_page_class_init,
			(GtkObjectInitFunc) eog_preview_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		page_type = gtk_type_unique (PARENT_TYPE, &page_info);
	}

	return (page_type);
}

GtkWidget*
eog_preview_page_new (EogImageView *image_view, gint col, gint row)
{
	EogPreviewPage *page;

	page = gtk_type_new (EOG_TYPE_PREVIEW_PAGE);
	page->priv->image_view = image_view;
	page->priv->col = col;
	page->priv->row = row;

	page->priv->group = gnome_canvas_item_new (
				gnome_canvas_root (GNOME_CANVAS (page)), 
				gnome_canvas_group_get_type (), 
				"x", 0.0, "y", 0.0, NULL);
	
	return (GTK_WIDGET (page));
}
