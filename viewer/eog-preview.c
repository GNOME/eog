#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-preview.h>

#define PARENT_TYPE GNOME_TYPE_CANVAS
static GnomeCanvasClass *parent_class = NULL;

struct _EogPreviewPrivate
{
	GdkPixbuf	*pixbuf;

	EogPreview	*above;
	EogPreview	*below;
	EogPreview	*right;
	EogPreview	*left;

	gint		 col;
	gint		 row;

	GConfClient	*client;
	guint		 notify_orientation;
	guint		 notify_paper;
	guint		 notify_left;
	guint		 notify_right;
	guint		 notify_bottom;
	guint		 notify_top;
	guint		 notify_adjust;
	guint		 notify_fit;
	guint		 notify_horizontally;
	guint		 notify_vertically;

	GnomeCanvasItem	*group;
	GnomeCanvasItem	*bg_white;
	GnomeCanvasItem	*bg_black;
	GnomeCanvasItem *image;

	double		 width;
	double		 height;

	gint		 top;
	gint		 bottom;
	gint		 right_margin;
	gint		 left_margin;

	gint		 adjust_to;
	gboolean	 fit_to_page;

	gboolean	 vertically;
	gboolean	 horizontally;
};

#define SCALE(param) (0.15 * param)
#define OFFSET_X 2.0
#define OFFSET_Y 2.0

#define CHECK_INT(c,x,y) {gint i = y; if (i != x) c = TRUE; x = i;}
#define CHECK_BOOL(c,x,y) {gboolean b = y; if (b != x) c = TRUE; x = b;}

static EogPreview*
get_root_page (EogPreview *page)
{
	while (page->priv->left)
		page = page->priv->left;
	while (page->priv->above)
		page = page->priv->above;
	return (page);
}

static gboolean
remove_page (gpointer data)
{
	GtkWidget 	*vbox;
	GtkWidget	*hbox;
	EogPreview	*page;

	page = EOG_PREVIEW (data);

	/* Don't remove (0, 0)! */
	if ((page->priv->col == 0) && (page->priv->row == 0))
		return (FALSE);

	if (page->priv->left)
		page->priv->left->priv->right = NULL;
	if (page->priv->right)
		page->priv->right->priv->left = NULL;
	if (page->priv->above)
		page->priv->above->priv->below = NULL;
	if (page->priv->below)
		page->priv->below->priv->above = NULL;

	/* Remove the page (myself!) */
	vbox = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_VBOX);
	gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (page));
	
	/* If empty, remove VBox */
	hbox = gtk_widget_get_ancestor (GTK_WIDGET (vbox), GTK_TYPE_HBOX);
	if (!gtk_container_children (GTK_CONTAINER (vbox)))
		gtk_container_remove (GTK_CONTAINER (hbox), vbox);

	return (FALSE);
}

static gboolean
settings_changed (EogPreview *page)
{
	gboolean changed = FALSE;

	CHECK_INT (changed, page->priv->top, 
		   SCALE (gconf_client_get_int (page->priv->client,
					"/apps/eog/viewer/top", NULL)));
	CHECK_INT (changed, page->priv->bottom, 
		   SCALE (gconf_client_get_int (page->priv->client,
					"/apps/eog/viewer/bottom", NULL)));
	CHECK_INT (changed, page->priv->right_margin,
		   SCALE (gconf_client_get_int (page->priv->client,
					"/apps/eog/viewer/right", NULL)));
	CHECK_INT (changed, page->priv->left_margin, 
		   SCALE (gconf_client_get_int (page->priv->client,
					"/apps/eog/viewer/left", NULL)));
	CHECK_BOOL (changed, page->priv->fit_to_page,
		    gconf_client_get_bool (page->priv->client,
					"/apps/eog/viewer/fit_to_page", NULL));
	CHECK_INT (changed, page->priv->adjust_to,
		   gconf_client_get_int (page->priv->client,
					"/apps/eog/viewer/adjust_to", NULL));
	CHECK_BOOL (changed, page->priv->vertically,
		    gconf_client_get_bool (page->priv->client,
					"/apps/eog/viewer/vertically", NULL));
	CHECK_BOOL (changed, page->priv->horizontally,
		    gconf_client_get_bool (page->priv->client,
					"/apps/eog/viewer/horizontally", NULL));
	return (changed);
}

static gboolean
create_page_right (gpointer data)
{
	EogPreview 	*page;
	EogPreview	*root;
	EogPreview	*new;
	GtkWidget 	*vbox;
	GtkWidget 	*hbox;
	GList	  	*children;
	gint		 i;

	page = EOG_PREVIEW (data);

	root = get_root_page (page);
	hbox = gtk_widget_get_ancestor (GTK_WIDGET (root), GTK_TYPE_HBOX);
	g_return_val_if_fail (hbox, FALSE);

	children = gtk_container_children (GTK_CONTAINER (hbox));
	if (g_list_length (children) <= page->priv->col + 1) {
		vbox = gtk_vbox_new (TRUE, 8);
		gtk_widget_show (vbox);
		gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	} else
		vbox = g_list_nth_data (children, page->priv->col + 1);
	
	new = EOG_PREVIEW (eog_preview_new (
					page->priv->client, page->priv->pixbuf,
					page->priv->col + 1, page->priv->row));
	gtk_widget_show (GTK_WIDGET (new));
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (new), FALSE, FALSE, 0);

	/* Bring the pages in correct order */
	children = gtk_container_children (GTK_CONTAINER (vbox));
	for (i = 0; i < g_list_length (children); i++) {
		EogPreview *page;

		page = EOG_PREVIEW (g_list_nth_data (children, i));
		gtk_box_reorder_child (GTK_BOX (vbox), GTK_WIDGET (page), 
			       page->priv->row);
	}

	new->priv->left = page;
	page->priv->right = new;
	if (page->priv->above && page->priv->above->priv->right) {
		g_return_val_if_fail (
			!page->priv->above->priv->right->priv->below, FALSE);
		new->priv->above = page->priv->above->priv->right;
		page->priv->above->priv->right->priv->below = new;
	}

	return (FALSE);
}

static gboolean
create_page_below (gpointer data)
{
	EogPreview 	*page;
	EogPreview	*root;
	GtkWidget 	*vbox;
	EogPreview 	*new;

	page = EOG_PREVIEW (data);

	root = get_root_page (page);
	vbox = gtk_widget_get_ancestor (GTK_WIDGET (root), GTK_TYPE_VBOX);
	g_return_val_if_fail (vbox, FALSE);

	new = EOG_PREVIEW (eog_preview_new (
					page->priv->client, page->priv->pixbuf, 
					page->priv->col, page->priv->row + 1));
	gtk_widget_show (GTK_WIDGET (new));
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (new), FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (vbox), GTK_WIDGET (new), 
			       page->priv->row + 1);
	
	new->priv->above = page;
	page->priv->below = new;
	if (page->priv->left && page->priv->left->priv->below) {
		g_return_val_if_fail (
			!page->priv->left->priv->below->priv->right, FALSE);
		page->priv->left->priv->below->priv->right = new;
		new->priv->left = page->priv->left->priv->below;
	}

	return (FALSE);
}

static void
adjust_image (EogPreview *page)
{
	GtkWidget	*widget;
	GdkPixmap	*pixmap;
	GdkBitmap	*bitmap;
	GdkPixbuf	*pixbuf;
	gint		 pixbuf_width, pixbuf_height;
	gint		 image_width, image_height;
	double		 available_x, available_y;
	double		 x, y;

	/* How big is the pixbuf? */
	pixbuf_width = SCALE (gdk_pixbuf_get_width (page->priv->pixbuf));
	pixbuf_height = SCALE (gdk_pixbuf_get_height (page->priv->pixbuf));
	g_return_if_fail (pixbuf_width > 0);
	g_return_if_fail (pixbuf_height > 0);

	/* How much place do we have got on the paper? */
	available_x = page->priv->width - page->priv->left_margin 
					- page->priv->right_margin;
	available_y = page->priv->height - page->priv->top - page->priv->bottom;
	g_return_if_fail (available_x > 0);
	g_return_if_fail (available_y > 0);

	/* Shall we fit the image onto one page? */
	if (page->priv->fit_to_page) {
		double prop_paper, prop_pixbuf;

		/* Fit the image onto the page (0, 0). 	*/
		/* If we aren't page (0, 0), remove us.	*/
		if (page->priv->col != 0 || page->priv->row != 0) {
			gtk_idle_add (remove_page, page);
			return;
		}

		prop_paper = available_y / available_x;
		prop_pixbuf = (double) pixbuf_height / pixbuf_width;

		/* Calculate width and height of image */
		if (prop_pixbuf > prop_paper) {
			image_width = available_y / prop_pixbuf;
			image_height = available_y;
		} else {
			image_width = available_x;
			image_height = available_x * prop_pixbuf;
		}
		g_return_if_fail (image_width > 0);
		g_return_if_fail (image_height > 0);

		pixbuf = gdk_pixbuf_scale_simple (page->priv->pixbuf,
						  image_width, image_height,
						  GDK_INTERP_NEAREST);
	} else {
		GdkPixbuf *adjusted_pixbuf;
		gint	   adjusted_width;
		gint	   adjusted_height;

		adjusted_width = pixbuf_width * page->priv->adjust_to / 100;
		adjusted_height = pixbuf_height * page->priv->adjust_to / 100;

		adjusted_pixbuf = gdk_pixbuf_scale_simple (page->priv->pixbuf,
							   adjusted_width,
							   adjusted_height,
							   GDK_INTERP_NEAREST);
	
		/* Width of image? */
		if (adjusted_width > available_x * (page->priv->col + 1)) {
			image_width = available_x;
			if (!page->priv->right) {
				gtk_idle_add (create_page_right, page);
			}
				
		} else {
			image_width = adjusted_width - available_x * 
							page->priv->col;
		}

		/* Height of image? */
		if (adjusted_height > available_y * (page->priv->row + 1)) {
			image_height = available_y;
			if (!page->priv->below && page->priv->col == 0) {
				gtk_idle_add (create_page_below, page);
			}
		} else {
			image_height = adjusted_height - available_y * 
							page->priv->row;
		}

		if ((image_height <= 0) || (image_width <= 0)) {
			gtk_idle_add (remove_page, page);
			return;
		}

		pixbuf = gdk_pixbuf_new (
			gdk_pixbuf_get_colorspace (adjusted_pixbuf),
			gdk_pixbuf_get_has_alpha (adjusted_pixbuf),
			gdk_pixbuf_get_bits_per_sample (adjusted_pixbuf),
			image_width,
			image_height);

		gdk_pixbuf_copy_area (adjusted_pixbuf,
				      page->priv->col * available_x,
				      page->priv->row * available_y,
				      image_width,
				      image_height,
				      pixbuf, 0, 0);

		gdk_pixbuf_unref (adjusted_pixbuf);
	}

	if (page->priv->image)
		gtk_object_destroy (GTK_OBJECT (page->priv->image));

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 1);
	gdk_pixbuf_unref (pixbuf);
	widget = gtk_pixmap_new (pixmap, bitmap);
	gtk_widget_show (widget);

	/* Where to put the image? */
	x = OFFSET_X + page->priv->left_margin;
	if (page->priv->horizontally)
		x += (page->priv->width - page->priv->left_margin
					- page->priv->right_margin 
					- image_width) / 2;
	y = OFFSET_Y + page->priv->top;
	if (page->priv->vertically)
		y += (page->priv->height - page->priv->bottom 
					 - page->priv->top 
					 - image_height) / 2;

	page->priv->image = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (page->priv->group), 
				gnome_canvas_widget_get_type (), 
				"widget", widget, 
				"x", x,
				"y", y,
				"width", (double) image_width,
				"height", (double) image_height,
				"anchor", GTK_ANCHOR_NORTH_WEST, 
				"size_pixels", FALSE, NULL);
}

static void
adjust_paper (EogPreview *page)
{
	const GnomePaper *paper;
	gchar		 *paper_size;
	gboolean	  landscape;

	paper_size = gconf_client_get_string (page->priv->client,
					      "/apps/eog/viewer/paper_size", 
					      NULL);
	if (!paper_size)
		paper_size = g_strdup (gnome_paper_name_default ());
	
	paper = gnome_paper_with_name (paper_size);
	g_free (paper_size);
	
	landscape = gconf_client_get_bool (page->priv->client,
					   "/apps/eog/viewer/landscape", NULL);
	if (landscape) {
		page->priv->width = SCALE (gnome_paper_psheight (paper));
		page->priv->height = SCALE (gnome_paper_pswidth (paper));
	} else {
		page->priv->height = SCALE (gnome_paper_psheight (paper));
		page->priv->width = SCALE (gnome_paper_pswidth (paper));
	}

	gnome_canvas_set_scroll_region (GNOME_CANVAS (page), 0.0, 0.0, 
				OFFSET_X + page->priv->width,
				OFFSET_Y + page->priv->height);
	gtk_widget_set_usize (GTK_WIDGET (page), 
			      OFFSET_X + page->priv->width,
			      OFFSET_Y + page->priv->height);

	/* gnome_canvas_item_scale is listed in gnome-canvas.h but not
	   implemented in gnome-canvas.c. How could that happen?!? */

	if (page->priv->bg_black)
		gtk_object_destroy (GTK_OBJECT (page->priv->bg_black));
	if (page->priv->bg_white)
		gtk_object_destroy (GTK_OBJECT (page->priv->bg_white));

	page->priv->bg_black = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (page->priv->group), 
			gnome_canvas_rect_get_type (), 
			"x1", 0.0, "y1", 0.0, 
			"x2", page->priv->width,
			"y2", page->priv->height,
			"fill_color", "black", 
			"outline_color", "black", 
			"width_pixels", 1, NULL);
	page->priv->bg_white = gnome_canvas_item_new (
			GNOME_CANVAS_GROUP (page->priv->group), 
			gnome_canvas_rect_get_type (), 
			"x1", OFFSET_X, "y1", OFFSET_Y, 
			"x2", OFFSET_X + page->priv->width, 
			"y2", OFFSET_Y + page->priv->height, 
			"fill_color", "white", 
			"outline_color", "black",
			"width_pixels", 1, NULL);

	adjust_image (page);
}

static void
notify_paper_and_orientation (GConfClient *client, guint cnxn,
			      GConfEntry *entry, gpointer data)
{
	EogPreview *page;

	page = EOG_PREVIEW (data);

	adjust_paper (page);
}

static gboolean
adjust_image_idle (gpointer data)
{
	EogPreview *page;

	page = EOG_PREVIEW (data);
	
	if (settings_changed (page))
		adjust_image (page);

	return (FALSE);
}

static void
notify_image (GConfClient *client, guint cnxn, GConfEntry *entry, 
	      gpointer data)
{
	EogPreview *page;

	page = EOG_PREVIEW (data);

	gtk_idle_add (adjust_image_idle, page);
}

static void
eog_preview_destroy (GtkObject *object)
{
	EogPreview	*page;

	page = EOG_PREVIEW (object);

	gconf_client_notify_remove (page->priv->client, 
				    page->priv->notify_orientation);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_paper);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_right);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_left);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_top);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_bottom);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_fit);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_adjust);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_vertically);
	gconf_client_notify_remove (page->priv->client,
				    page->priv->notify_horizontally);

	gtk_object_unref (GTK_OBJECT (page->priv->client));

	gdk_pixbuf_unref (page->priv->pixbuf);

	g_free (page->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
eog_preview_init (EogPreview *page)
{
	page->priv = g_new0 (EogPreviewPrivate, 1);
}

static void
eog_preview_class_init (EogPreviewClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = eog_preview_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
}

GtkType
eog_preview_get_type (void)
{
	static GtkType page_type = 0;

	if (!page_type) {
		static const GtkTypeInfo page_info = {
			"EogPreview",
			sizeof (EogPreview),
			sizeof (EogPreviewClass),
			(GtkClassInitFunc) eog_preview_class_init,
			(GtkObjectInitFunc) eog_preview_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		page_type = gtk_type_unique (PARENT_TYPE, &page_info);
	}

	return (page_type);
}

GtkWidget*
eog_preview_new (GConfClient *client, GdkPixbuf *pixbuf, 
		      gint col, gint row)
{
	EogPreview	*page;

	page = gtk_type_new (EOG_TYPE_PREVIEW);

	page->priv->client = client;
	gtk_object_ref (GTK_OBJECT (client));

	page->priv->pixbuf = gdk_pixbuf_ref (pixbuf);

	page->priv->row = row;
	page->priv->col = col;

	page->priv->group = gnome_canvas_item_new (
				gnome_canvas_root (GNOME_CANVAS (page)),
				gnome_canvas_group_get_type (),
				"x", 0.0, "y", 0.0, NULL);

	settings_changed (page);
	adjust_paper (page);

	page->priv->notify_orientation = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/landscape",
				notify_paper_and_orientation, page, NULL, NULL);
	page->priv->notify_paper = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/paper_size",
				notify_paper_and_orientation, page, NULL, NULL);
	page->priv->notify_top = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/top",
				notify_image, page, NULL, NULL);
	page->priv->notify_bottom = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/bottom", 
				notify_image, page, NULL, NULL);
	page->priv->notify_right = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/right", 
				notify_image, page, NULL, NULL);
	page->priv->notify_left = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/left", 
				notify_image, page, NULL, NULL);
	page->priv->notify_adjust = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/adjust_to",
				notify_image, page, NULL, NULL);
	page->priv->notify_fit = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/fit_to_page",
				notify_image, page, NULL, NULL);
	page->priv->notify_horizontally = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/horizontally",
				notify_image, page, NULL, NULL);
	page->priv->notify_vertically = gconf_client_notify_add (
				page->priv->client,
				"/apps/eog/viewer/vertically",
				notify_image, page, NULL, NULL);

	return (GTK_WIDGET (page));
}

