#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-preview-page.h>

#define PARENT_TYPE GNOME_TYPE_CANVAS
static GnomeCanvasClass *parent_class = NULL;

struct _EogPreviewPagePrivate
{
	EogImageView	 *image_view;

	gint		  col;
	gint		  row;

	GnomeCanvasGroup *group;
	
	GnomeCanvasItem  *bg_white;
	GnomeCanvasItem  *bg_black;
	GnomeCanvasItem  *image;

	GnomeCanvasItem  *margin_top;
	GnomeCanvasItem	 *margin_bottom;
	GnomeCanvasItem  *margin_right;
	GnomeCanvasItem	 *margin_left;

	GnomeCanvasGroup *cut_group;
	GnomeCanvasItem  *cut_top_right;
	GnomeCanvasItem  *cut_top_left;
	GnomeCanvasItem  *cut_bottom_right;
	GnomeCanvasItem  *cut_bottom_left;
	GnomeCanvasItem  *cut_right_bottom;
	GnomeCanvasItem  *cut_right_top;
	GnomeCanvasItem  *cut_left_bottom;
	GnomeCanvasItem  *cut_left_top;
	
	gint		  width;
	gint		  height;

	gint		  top;
	gint		  bottom;
	gint		  left;
	gint		  right;
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
		page->priv->group,
		gnome_canvas_rect_get_type (), 
		"x1", 0.0, "y1", 0.0, 
		"x2", (double) width, 
		"y2", (double) height, 
		"fill_color", "black", 
		"outline_color", "black", 
		"width_pixels", 1, NULL);
	page->priv->bg_white = gnome_canvas_item_new (
		page->priv->group, 
		gnome_canvas_rect_get_type (), 
		"x1", (double) OFFSET_X, 
		"y1", (double) OFFSET_Y, 
		"x2", (double) OFFSET_X + width - 1, 
		"y2", (double) OFFSET_Y + height - 1, 
		"fill_color", "white", 
		"outline_color", "white", 
		"width_pixels", 1, NULL); 
}

static void
move_line (GnomeCanvasItem *item, gint x1, gint y1, gint x2, gint y2)
{
	GnomeCanvasPoints *points; 
	
	points = gnome_canvas_points_new (2); 
	points->coords [0] = (double) OFFSET_X + x1; 
	points->coords [1] = (double) OFFSET_Y + y1; 
	points->coords [2] = (double) OFFSET_X + x2; 
	points->coords [3] = (double) OFFSET_Y + y2; 
	
	gnome_canvas_item_set (item, "points", points, NULL); 
	gnome_canvas_points_unref (points); 
	gnome_canvas_item_raise_to_top (item);
}

static void
redraw_cut (EogPreviewPage *page, gint width, gint height, gint x, gint y,
	    gint image_width, gint image_height, gboolean cut)
{
	/* Do we have to display the cutting help? */
	if (!cut) {
		gnome_canvas_item_lower_to_bottom (
			GNOME_CANVAS_ITEM (page->priv->cut_group));
		return;
	}

	move_line (page->priv->cut_top_left, x, 0, x, 2 * y / 3);
	move_line (page->priv->cut_top_right, x + image_width - 1, 0, 
		   x + image_width - 1, 2 * y / 3);
	move_line (page->priv->cut_bottom_left, 
		   x, y + image_height  - 1 + (height - y - image_height) / 3, 
		   x, height);
	move_line (page->priv->cut_bottom_right, x + image_width - 1, 
		   y + image_height - 1 + (height - y - image_height) / 3,
		   x + image_width - 1, height);
	move_line (page->priv->cut_left_top, 
		   0, y, 2 * x / 3, y);
	move_line (page->priv->cut_left_bottom, 
		   0, y + image_height - 1, 2 * x / 3, y + image_height - 1);
	move_line (page->priv->cut_right_top, 
		   x + image_width + (width - x - image_width) / 3, y, 
		   width, y);
	move_line (page->priv->cut_right_bottom, 
		   x + image_width + (width - x - image_width) / 3, 
		   y + image_height - 1, width, y + image_height - 1);

	gnome_canvas_item_raise_to_top (
		GNOME_CANVAS_ITEM (page->priv->cut_group));
}

static void
redraw_margins (EogPreviewPage *page, gint top, gint bottom, 
		gint left, gint right, gint width, gint height)
{
	move_line (page->priv->margin_top, 0, top, width, top);
	move_line (page->priv->margin_bottom, 0, height - bottom - 1, 
		   width, height - bottom - 1);
	move_line (page->priv->margin_left, left, 0, left, height);
	move_line (page->priv->margin_right, width - right - 1, 0, 
		   width - right - 1, height);
}

void
eog_preview_page_update (EogPreviewPage *page, GdkPixbuf *pixbuf,
			 gint width, gint height, 
			 gint bottom, gint top, gint right, gint left,
			 gboolean vertically, gboolean horizontally, 
			 gboolean cut,
			 gint *cols_needed, gint *rows_needed)
{
	GtkWidget		*widget;
	GdkPixbuf		*pixbuf_to_show;
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

	/* How big is the pixbuf? */
	pixbuf_width = gdk_pixbuf_get_width (pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (pixbuf);

	/* Calculate the free place on the paper */
	leftover_width = avail_width - (pixbuf_width % avail_width);
	leftover_width = leftover_width % avail_width;
	leftover_height = avail_height - (pixbuf_height % avail_height);
	leftover_height = leftover_height % avail_height;

	*cols_needed = (pixbuf_width + leftover_width) / avail_width;
	*rows_needed = (pixbuf_height + leftover_height) / avail_height;
	
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
		image_width = pixbuf_width;
	else if (last_x) {
		image_width = ((pixbuf_width - 1) % avail_width) + 1;
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
		image_height = pixbuf_height; 
	else if (last_y) { 
		image_height = ((pixbuf_height - 1) % avail_height) + 1;
		if (vertically) 
			image_height += leftover_height / 2; 
	} else { 
		image_height = avail_height; 
		if (first_y && vertically) 
			image_height -= leftover_height / 2; 
	} 
	g_return_if_fail (image_height > 0); 

	pixbuf_to_show = gdk_pixbuf_new (
			gdk_pixbuf_get_colorspace (pixbuf),
			gdk_pixbuf_get_has_alpha (pixbuf), 
			gdk_pixbuf_get_bits_per_sample (pixbuf), 
			image_width, image_height); 
	
	/* Where do we begin to copy (x)? */ 
	if (first_x) 
		x = 0; 
	else if (last_x) 
		x = pixbuf_width - image_width; 
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
		y = pixbuf_height - image_height; 
	else { 
		y = avail_height * page->priv->row; 
		if (vertically) 
			y -= leftover_height / 2; 
	} 
	g_return_if_fail (y >= 0); 
	
	gdk_pixbuf_copy_area (pixbuf, x, y, 
			      image_width, image_height, pixbuf_to_show, 0, 0);

	gdk_pixbuf_render_pixmap_and_mask (pixbuf_to_show, &pixmap, &bitmap, 1);
	gdk_pixbuf_unref (pixbuf_to_show); 
	widget = gtk_pixmap_new (pixmap, bitmap); 
	gtk_widget_show (widget);

	/* Where to put the image (x)? */ 
	x = left; 
	if (horizontally && first_x)
	    	x += leftover_width / 2; 
	
	/* Where to put the image (y)? */ 
	y = top; 
	if (vertically && first_y) 
	    	y += leftover_height / 2; 
	
	if (page->priv->image) 
		gtk_object_destroy (GTK_OBJECT (page->priv->image)); 
	
	page->priv->image = gnome_canvas_item_new (
					page->priv->group, 
					gnome_canvas_widget_get_type (), 
					"widget", widget, 
					"x", (double) OFFSET_X + x, 
					"y", (double) OFFSET_Y + y, 
					"width", (double) image_width, 
					"height", (double) image_height, 
					"anchor", GTK_ANCHOR_NORTH_WEST, 
					"size_pixels", TRUE, NULL);
	if ((top != page->priv->top) || (bottom != page->priv->bottom) ||
	    (left != page->priv->left) || (right != page->priv->right))
		redraw_margins (page, top, bottom, left, right, width, height);
	
	redraw_cut (page, width, height, x, y, image_width, image_height, cut);
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

static GnomeCanvasItem*
make_line (GnomeCanvasGroup *group, const gchar *color)
{ 
	GnomeCanvasPoints *points; 
	GnomeCanvasItem   *item; 
	
	points = gnome_canvas_points_new (2); 
	points->coords [0] = 0.0; 
	points->coords [1] = 0.0; 
	points->coords [2] = 0.0; 
	points->coords [3] = 0.0; 
	
	item = gnome_canvas_item_new (group,
				      gnome_canvas_line_get_type (), 
				      "points", points, 
				      "width_pixels", 1, 
				      "fill_color", color,
				      NULL); 
	gnome_canvas_points_unref (points); 
	
	return (item); 
}

GtkWidget*
eog_preview_page_new (EogImageView *image_view, gint col, gint row)
{
	EogPreviewPage  	*page;
	EogPreviewPagePrivate 	*priv;
	GnomeCanvasItem 	*group;

	page = gtk_type_new (EOG_TYPE_PREVIEW_PAGE);

	priv = page->priv;
	priv->image_view = image_view;
	priv->col = col;
	priv->row = row;

	group = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (page)), 
				       gnome_canvas_group_get_type (), 
				       "x", 0.0, "y", 0.0, NULL);
	priv->group = GNOME_CANVAS_GROUP (group);
	
	priv->margin_top = make_line (page->priv->group, "red");
	priv->margin_bottom = make_line (page->priv->group, "red");
	priv->margin_right = make_line (page->priv->group, "red");
	priv->margin_left = make_line (page->priv->group, "red");

	group = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (page)),
				       gnome_canvas_group_get_type (),
				       "x", 0.0, "y", 0.0, NULL);
	priv->cut_group = GNOME_CANVAS_GROUP (group);

	priv->cut_left_top = make_line (page->priv->cut_group, "black");
	priv->cut_left_bottom = make_line (page->priv->cut_group, "black");
	priv->cut_right_top = make_line (page->priv->cut_group, "black");
	priv->cut_right_bottom = make_line (page->priv->cut_group, "black");
	priv->cut_top_right = make_line (page->priv->cut_group, "black");
	priv->cut_top_left = make_line (page->priv->cut_group, "black");
	priv->cut_bottom_right = make_line (page->priv->cut_group, "black");
	priv->cut_bottom_left = make_line (page->priv->cut_group, "black");
	
	return (GTK_WIDGET (page));
}
