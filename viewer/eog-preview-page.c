#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-preview-page.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

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

	GnomeCanvasGroup *margin_group;
	GnomeCanvasItem  *margin_top;
	GnomeCanvasItem	 *margin_bottom;
	GnomeCanvasItem  *margin_right;
	GnomeCanvasItem	 *margin_left;

	GnomeCanvasGroup *overlap_group;
	GnomeCanvasItem	 *overlap_right;
	GnomeCanvasItem	 *overlap_bottom;

	GnomeCanvasGroup *cut_group;
	GnomeCanvasItem  *cut_top_right;
	GnomeCanvasItem  *cut_top_left;
	GnomeCanvasItem  *cut_bottom_right;
	GnomeCanvasItem  *cut_bottom_left;
	GnomeCanvasItem  *cut_right_bottom;
	GnomeCanvasItem  *cut_right_top;
	GnomeCanvasItem  *cut_left_bottom;
	GnomeCanvasItem  *cut_left_top;
	
	gdouble		  width;
	gdouble		  height;
};

#define OFFSET_X 2
#define OFFSET_Y 2

static void
resize_paper (EogPreviewPage *page)
{
	gnome_canvas_set_scroll_region (GNOME_CANVAS (page), 0.0, 0.0,
					OFFSET_X + page->priv->width, 
					OFFSET_Y + page->priv->height);
	gtk_widget_set_usize (GTK_WIDGET (page), OFFSET_X + page->priv->width,
			      OFFSET_Y + page->priv->height);
	
	if (page->priv->bg_black)
		gtk_object_destroy (GTK_OBJECT (page->priv->bg_black));
	if (page->priv->bg_white)
		gtk_object_destroy (GTK_OBJECT (page->priv->bg_white));
	
	page->priv->bg_black = gnome_canvas_item_new (
		page->priv->group,
		gnome_canvas_rect_get_type (), 
		"x1", 0.0, "y1", 0.0, 
		"x2", (double) page->priv->width, 
		"y2", (double) page->priv->height, 
		"fill_color", "black", 
		"outline_color", "black", 
		"width_pixels", 1, NULL);
	page->priv->bg_white = gnome_canvas_item_new (
		page->priv->group, 
		gnome_canvas_rect_get_type (), 
		"x1", (double) OFFSET_X, 
		"y1", (double) OFFSET_Y, 
		"x2", (double) OFFSET_X + page->priv->width, 
		"y2", (double) OFFSET_Y + page->priv->height, 
		"fill_color", "white", 
		"outline_color", "white", 
		"width_pixels", 1, NULL); 
}

static void
move_line (GnomeCanvasItem *item, 
	   gdouble x1, gdouble y1, gdouble x2, gdouble y2)
{
	GnomeCanvasPoints *points; 
	
	points = gnome_canvas_points_new (2); 
	points->coords [0] = (double) OFFSET_X + x1; 
	points->coords [1] = (double) OFFSET_Y + y1; 
	points->coords [2] = (double) OFFSET_X + x2; 
	points->coords [3] = (double) OFFSET_Y + y2; 
	
	gnome_canvas_item_set (item, "points", points, NULL); 
	gnome_canvas_points_unref (points); 
}

static void
redraw_cut (EogPreviewPage *page, gdouble x, gdouble y,
	    gdouble image_width, gdouble image_height, gboolean cut)
{
	gdouble x1, y1, x2, y2, x3, y3;
	
	/* Do we have to display the cutting help? */
	if (!cut) {
		gnome_canvas_item_lower_to_bottom (
			GNOME_CANVAS_ITEM (page->priv->cut_group));
		move_line (page->priv->cut_right_top, 0.0, 0.0, 0.0, 0.0);
		move_line (page->priv->cut_right_bottom, 0.0, 0.0, 0.0, 0.0);
		move_line (page->priv->cut_bottom_left, 0.0, 0.0, 0.0, 0.0);
		move_line (page->priv->cut_bottom_right, 0.0, 0.0, 0.0, 0.0);
		return;
	}

	x1 = x + image_width;
	y1 = y + image_height;
	x2 = x1 + (page->priv->width - x - image_width) / 3;
	y2 = y1 + (page->priv->height - y - image_height) / 3;
	x3 = page->priv->width;
	y3 = page->priv->height;
	
	move_line (page->priv->cut_top_left, x, 0.0, x, 2 * y / 3);
	move_line (page->priv->cut_top_right, x1, 0.0, x1, 2 * y / 3);
	move_line (page->priv->cut_bottom_left, x, y2, x, y3);
	move_line (page->priv->cut_bottom_right, x1, y2, x1, y3);
	move_line (page->priv->cut_left_top, 0.0, y, 2 * x / 3, y);
	move_line (page->priv->cut_left_bottom, 0.0, y1, 2 * x / 3, y1);
	move_line (page->priv->cut_right_top, x2, y, x3, y);
	move_line (page->priv->cut_right_bottom, x2, y1, x3, y1);

	gnome_canvas_item_raise_to_top (
		GNOME_CANVAS_ITEM (page->priv->cut_group));
}

static void
redraw_overlap (EogPreviewPage *page, gdouble x, gdouble y, gdouble image_width,
		gdouble image_height, gboolean last_x, gboolean last_y,
		gdouble overlap_x, gdouble overlap_y, gboolean overlap)
{
	gdouble x1, y1;

	if (!overlap || last_x)
		move_line (page->priv->overlap_right, 0.0, 0.0, 0.0, 0.0);
	if (!overlap || last_y)
		move_line (page->priv->overlap_bottom, 0.0, 0.0, 0.0, 0.0);
	if (!overlap)
		gnome_canvas_item_lower_to_bottom ( 
			GNOME_CANVAS_ITEM (page->priv->overlap_group));
	else 
		gnome_canvas_item_raise_to_top (
			GNOME_CANVAS_ITEM (page->priv->overlap_group));

	if (overlap && !last_x) {
		x1 = x + image_width - overlap_x;
		y1 = y + image_height;
		move_line (page->priv->overlap_right, x1, y, x1, y1);
	}
	if (overlap && !last_y) {
		x1 = x + image_width;
		y1 = y + image_height - overlap_y;
		move_line (page->priv->overlap_bottom, x, y1, x1, y1);
	}
}

static void
redraw_margins (EogPreviewPage *page, gdouble top, gdouble bottom, 
		gdouble left, gdouble right)
{
	move_line (page->priv->margin_top, 0.0, top, page->priv->width, top);
	move_line (page->priv->margin_bottom, 0.0, page->priv->height - bottom, 
		   page->priv->width, page->priv->height - bottom);
	move_line (page->priv->margin_left, left, 0.0, left, 
		   page->priv->height);
	move_line (page->priv->margin_right, page->priv->width - right, 0.0, 
		   page->priv->width - right, page->priv->height);
	gnome_canvas_item_raise_to_top (
		GNOME_CANVAS_ITEM (page->priv->margin_group));
}

void
eog_preview_page_update (EogPreviewPage *page, GdkPixbuf *pixbuf,
			 gdouble width, gdouble height, gdouble bottom, 
			 gdouble top, gdouble right, gdouble left,
			 gboolean vertically, gboolean horizontally, 
			 gboolean cut, gdouble overlap_x, gdouble overlap_y,
			 gboolean overlap, gint *cols, gint *rows)
{
	GdkPixbuf		*pixbuf_to_show;
	gint			 pixbuf_width, pixbuf_height;
	gdouble			 image_width, image_height;
	gdouble			 avail_width, avail_height;
	gdouble			 leftover_width, leftover_height;
	gdouble			 x, y;
	gboolean		 first_x, last_x;
	gboolean		 first_y, last_y;

	/* In case anything goes wrong... */
	*cols = 1;
	*rows = 1;

	if ((width != page->priv->width) || (height != page->priv->height)) {
		page->priv->width = width;
		page->priv->height = height;
		resize_paper (page);
	}
	
	/* Destroy old image */
	if (page->priv->image) {
		gtk_object_destroy (GTK_OBJECT (page->priv->image));
		page->priv->image = NULL;
	}

	/* Redraw margins */
	redraw_margins (page, top, bottom, left, right);

	/* How much place do we have got on the paper? */
	avail_width = width - left - right - overlap_x;
	avail_height = height - top - bottom - overlap_y;
	if (avail_width < 0)
		avail_width = 0;
	if (avail_height < 0)
		avail_height = 0;

	/* If no pixbuf is given or if we can't show a pixbuf because    */
	/* there is no space available, we just redraw the cutting help. */
	if (!pixbuf || !avail_width || !avail_height) {
		redraw_overlap (page, 0, 0, 0, 0, FALSE, FALSE, 0, 0, FALSE);
		redraw_cut (page, 0, 0, 0, 0, FALSE);
		return;
	}

	/* How big is the pixbuf? */
	pixbuf_width = gdk_pixbuf_get_width (pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (pixbuf);

	/* Calculate the free place on the paper */
	for (*cols = 1; pixbuf_width > *cols * avail_width + overlap_x; 
								(*cols)++);
	leftover_width = *cols * avail_width + overlap_x - pixbuf_width;
	for (*rows = 1; pixbuf_height > *rows * avail_height + overlap_y; 
								(*rows)++);
	leftover_height = *rows * avail_height + overlap_y - pixbuf_height;

	g_return_if_fail (*cols > page->priv->col);
	g_return_if_fail (*rows > page->priv->row);

	first_x = (page->priv->col == 0);
	first_y = (page->priv->row == 0);
	last_x = (*cols == page->priv->col + 1);
	last_y = (*rows == page->priv->row + 1);

	/* Width of image? */
	if (first_x && last_x)
		image_width = pixbuf_width;
	else if (last_x) {
		image_width = pixbuf_width - (*cols - 1) * avail_width;
		if (horizontally)
			image_width += leftover_width / 2.0;
	} else {
		image_width = avail_width + overlap_x;
		if (first_x && horizontally)
			image_width -= leftover_width / 2.0;
	}
	if (image_width < 1.0)
		image_width++;

	/* Height of image? */
	if (first_y && last_y) 
		image_height = pixbuf_height; 
	else if (last_y) { 
		image_height = pixbuf_height - (*rows - 1) * avail_height;
		if (vertically) 
			image_height += leftover_height / 2.0; 
	} else { 
		image_height = avail_height + overlap_y; 
		if (first_y && vertically) 
			image_height -= leftover_height / 2.0; 
	} 
	if (image_height < 1.0)
		image_height++;

	pixbuf_to_show = gdk_pixbuf_new (
			gdk_pixbuf_get_colorspace (pixbuf),
			gdk_pixbuf_get_has_alpha (pixbuf), 
			gdk_pixbuf_get_bits_per_sample (pixbuf), 
			(gint) image_width, (gint) image_height); 
	
	/* Where do we begin to copy (x)? */ 
	if (first_x) 
		x = 0.0; 
	else if (last_x) 
		x = pixbuf_width - image_width; 
	else { 
		x = avail_width * page->priv->col; 
		if (horizontally) 
			x -= leftover_width / 2.0;
	} 
	g_return_if_fail (x >= 0.0); 

	/* Where do we begin to copy (y)? */ 
	if (first_y) 
		y = 0.0; 
	else if (last_y) 
		y = pixbuf_height - image_height; 
	else { 
		y = avail_height * page->priv->row; 
		if (vertically) 
			y -= leftover_height / 2.0;
	} 
	g_return_if_fail (y >= 0.0); 
	
	gdk_pixbuf_copy_area (pixbuf, (gint) x, (gint) y,
			      (gint) image_width, (gint) image_height, 
			      pixbuf_to_show, 0, 0);

	/* Where to put the image (x)? */
	x = left;
	if (horizontally && first_x)
	    	x += leftover_width / 2.0;
	
	/* Where to put the image (y)? */
	y = top;
	if (vertically && first_y)
	    	y += leftover_height / 2.0;
	
	page->priv->image = gnome_canvas_item_new (
					page->priv->group, 
					gnome_canvas_pixbuf_get_type (),
					"pixbuf", pixbuf_to_show, 
					"x", (double) OFFSET_X + x, 
					"y", (double) OFFSET_Y + y, 
					"width", (double) image_width + 1.0, 
					"height", (double) image_height + 1.0, 
					NULL);
	
	redraw_cut (page, x, y, image_width, image_height, cut);
	redraw_overlap (page, x, y, image_width, image_height, last_x, last_y, 
			overlap_x, overlap_y, overlap);
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

	/* Margins */
	group = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (page)),
				       gnome_canvas_group_get_type (),
				       "x", 0.0, "y", 0.0, NULL);
	priv->margin_group = GNOME_CANVAS_GROUP (group);
	priv->margin_top = make_line (priv->margin_group, "red");
	priv->margin_bottom = make_line (priv->margin_group, "red");
	priv->margin_right = make_line (priv->margin_group, "red");
	priv->margin_left = make_line (priv->margin_group, "red");

	/* Overlap */
	group = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (page)),
				       gnome_canvas_group_get_type (),
				       "x", 0.0, "y", 0.0, NULL);
	priv->overlap_group = GNOME_CANVAS_GROUP (group);
	priv->overlap_right = make_line (priv->overlap_group, "black");
	priv->overlap_bottom = make_line (priv->overlap_group, "black");

	/* Cutting help */
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
