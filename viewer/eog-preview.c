#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-preview.h>

#include <eog-preview-page.h>

#define PARENT_TYPE GTK_TYPE_HBOX
static GtkHBoxClass *parent_class = NULL;

struct _EogPreviewPrivate
{
	EogImageView	*image_view;

	GtkWidget	*root;

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
	guint		 notify_cut;

	gint		 width;
	gint		 height;

	gint		 top;
	gint		 bottom;
	gint		 right;
	gint		 left;

	gint		 adjust_to;
	gboolean	 fit_to_page;

	gboolean	 vertically;
	gboolean	 horizontally;

	gboolean	 cut;
};

#define SCALE(param) (0.15 * param)
#define CHECK_INT(c,x,y)   {gint z; z = y; if (x != z) c = TRUE; x = z;}
#define CHECK_BOOL(c,x,y)  {gboolean z; z = y; if (x != z) c = TRUE; x = z;}

static gboolean
settings_changed (EogPreview *preview)
{
	GConfClient 	 *client;
	gboolean	  changed;
	gboolean	  landscape;
	gchar		 *paper_size;
	gint		  new_width, new_height;
	const GnomePaper *paper;

	changed = FALSE;
	client = preview->priv->client;

	paper_size = gconf_client_get_string (client, 
					"/apps/eog/viewer/paper_size", NULL); 
        if (!paper_size) 
		paper_size = g_strdup (gnome_paper_name_default ()); 
	
	paper = gnome_paper_with_name (paper_size); 
	g_free (paper_size); 
	
	landscape = gconf_client_get_bool (client, 
					"/apps/eog/viewer/landscape", NULL); 
	if (landscape) { 
		new_width = SCALE (gnome_paper_psheight (paper)); 
		new_height = SCALE (gnome_paper_pswidth (paper)); 
	} else { 
		new_height = SCALE (gnome_paper_psheight (paper)); 
		new_width = SCALE (gnome_paper_pswidth (paper)); 
	}
	if ((new_width != preview->priv->width) ||
	    (new_height != preview->priv->height))
		changed = TRUE;
	
	preview->priv->width = new_width;
	preview->priv->height = new_height;

	CHECK_INT (changed, preview->priv->top, 
		   SCALE (gconf_client_get_int (client,
					"/apps/eog/viewer/top", NULL)));
	CHECK_INT (changed, preview->priv->bottom,
		   SCALE (gconf_client_get_int (client,
					"/apps/eog/viewer/bottom", NULL)));
	CHECK_INT (changed, preview->priv->right,
		   SCALE (gconf_client_get_int (client,
					"/apps/eog/viewer/right", NULL)));
	CHECK_INT (changed, preview->priv->left,
		     SCALE (gconf_client_get_int (client,
					"/apps/eog/viewer/left", NULL)));
	CHECK_BOOL (changed, preview->priv->fit_to_page, 
		    gconf_client_get_bool (client,
					"/apps/eog/viewer/fit_to_page", NULL));
	CHECK_INT (changed, preview->priv->adjust_to, 
		   gconf_client_get_int (client,
					"/apps/eog/viewer/adjust_to", NULL));
	CHECK_BOOL (changed, preview->priv->vertically, 
		    gconf_client_get_bool (client,
					"/apps/eog/viewer/vertically", NULL));
	CHECK_BOOL (changed, preview->priv->horizontally,
		    gconf_client_get_bool (client,
					"/apps/eog/viewer/horizontally", NULL));
	CHECK_BOOL (changed, preview->priv->cut,
		    gconf_client_get_bool (client,
		    			"/apps/eog/viewer/cut", NULL));
	
	return (changed);
}

static void
create_vbox (EogPreview *preview, gint col, gint rows_needed)
{
	GtkWidget *vbox;
	GtkWidget *page;
	gint 	   i;

	vbox = gtk_vbox_new (TRUE, 8); 
	gtk_widget_show (vbox); 
	gtk_box_pack_start (GTK_BOX (preview), vbox, TRUE, TRUE, 0); 
	gtk_container_set_resize_mode (GTK_CONTAINER (vbox), GTK_RESIZE_PARENT);

	for (i = 0; i < rows_needed; i++) {
		page = eog_preview_page_new (preview->priv->image_view, col, i);
		gtk_widget_show (page);
		gtk_box_pack_start (GTK_BOX (vbox), page, TRUE, TRUE, 0);
	}
}

static void
update (EogPreview *preview)
{
	BonoboPropertyBag *bag;
	BonoboArg	  *arg;
	EogImage	  *image;
	GtkWidget 	  *page;
	GtkWidget 	  *vbox;
	GdkPixbuf	  *pixbuf;
	GdkPixbuf	  *pixbuf_orig;
	GdkInterpType	   interp;
	GList	  	  *children;
	GList	  	  *vbox_children;
	gint	  	   cols_needed, rows_needed;
	gint	  	   i, j;
	gint		   width, height;
	gint		   pixbuf_width, pixbuf_height;

	/* Get the pixbuf */
	image = eog_image_view_get_image (preview->priv->image_view);
	pixbuf_orig = eog_image_get_pixbuf (image);
	bonobo_object_unref (BONOBO_OBJECT (image));
	g_return_if_fail (pixbuf_orig);

	/* Get the size of the pixbuf */
	pixbuf_width = gdk_pixbuf_get_width (pixbuf_orig);
	pixbuf_height = gdk_pixbuf_get_height (pixbuf_orig);


	/* Get the interpolation type */
	bag = eog_image_view_get_property_bag (preview->priv->image_view);
	arg = bonobo_property_bag_get_value (bag, "interpolation", NULL);
	bonobo_object_unref (BONOBO_OBJECT (bag)); 
	g_return_if_fail (arg); 
	switch (*(GNOME_EOG_Interpolation*)arg->_value) { 
	case GNOME_EOG_INTERPOLATION_NEAREST: 
		interp = GDK_INTERP_NEAREST; 
		break; 
	case GNOME_EOG_INTERPOLATION_TILES: 
		interp = GDK_INTERP_TILES; 
		break; 
	case GNOME_EOG_INTERPOLATION_BILINEAR: 
		interp = GDK_INTERP_BILINEAR; 
		break; 
	case GNOME_EOG_INTERPOLATION_HYPERBOLIC: 
		interp = GDK_INTERP_HYPER; 
		break; 
	default: 
		g_warning ("Got unknow interpolation type!"); 
		interp = GDK_INTERP_NEAREST; 
	} 
	bonobo_arg_release (arg); 

	/* Calculate width and height of image */
	if (preview->priv->fit_to_page) {
		double prop_paper, prop_pixbuf;
		gint   avail_width, avail_height;

		avail_width = preview->priv->width - preview->priv->right 
						   - preview->priv->left;
		avail_height = preview->priv->height - preview->priv->top 
						     - preview->priv->bottom;
		prop_paper = (double) avail_height / avail_width;
		prop_pixbuf = (double) pixbuf_height / pixbuf_width;

		if (prop_pixbuf > prop_paper) {
			width = avail_height / prop_pixbuf;
			height = avail_height;
		} else {
			width = avail_width;
			height = avail_width * prop_pixbuf;
		}
	} else {
		width = SCALE (pixbuf_width * preview->priv->adjust_to / 100);
		height = SCALE (pixbuf_height * preview->priv->adjust_to / 100);
	}
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);

	/* Scale the pixbuf */
	pixbuf = gdk_pixbuf_scale_simple (pixbuf_orig, width, height, interp);
	gdk_pixbuf_unref (pixbuf_orig);

	/* Update the page (0, 0) in order to see how many pages we need */
	eog_preview_page_update (EOG_PREVIEW_PAGE (preview->priv->root), 
				 pixbuf,
				 preview->priv->width, preview->priv->height,
				 preview->priv->bottom, preview->priv->top,
				 preview->priv->right, preview->priv->left,
				 preview->priv->vertically,
				 preview->priv->horizontally,
				 preview->priv->cut,
				 &cols_needed, &rows_needed);

	/* Do we need to remove VBoxes? */
	children = gtk_container_children (GTK_CONTAINER (preview));
	for (i = g_list_length (children) - 1; i >= cols_needed; i--) {
		vbox = g_list_nth_data (children, i);
		gtk_container_remove (GTK_CONTAINER (preview), vbox);
	}

	/* Do we need to add or remove pages (rows)? */
	children = gtk_container_children (GTK_CONTAINER (preview));
	for (i = 0; i < g_list_length (children); i++) {
		vbox = g_list_nth_data (children, i);
		vbox_children = gtk_container_children (GTK_CONTAINER (vbox));

		/* Removing */
		for (j = g_list_length (vbox_children); j > rows_needed; j--) {
			page = g_list_nth_data (vbox_children, j - 1);
			gtk_container_remove (GTK_CONTAINER (vbox), page);
		}

		/* Adding */
		for (j = g_list_length (vbox_children); j < rows_needed; j++) {
			page = eog_preview_page_new (preview->priv->image_view,
						     i, j);
			gtk_widget_show (page);
			gtk_box_pack_start (GTK_BOX (vbox), page, 
					    TRUE, TRUE, 0);
		}
	}

	/* Do we need additional VBoxes? */
	children = gtk_container_children (GTK_CONTAINER (preview));
	for (i = g_list_length (children); i < cols_needed; i++)
		create_vbox (preview, i, rows_needed);

	/* Update all pages */
	children = gtk_container_children (GTK_CONTAINER (preview));
	for (i = 0; i < g_list_length (children); i++) {
		vbox = g_list_nth_data (children, i);
		vbox_children = gtk_container_children (GTK_CONTAINER (vbox));
		for (j = 0; j < g_list_length (vbox_children); j++) {
			page = g_list_nth_data (vbox_children, j);
			eog_preview_page_update (EOG_PREVIEW_PAGE (page),
						 pixbuf,
						 preview->priv->width,
						 preview->priv->height,
						 preview->priv->bottom,
						 preview->priv->top,
						 preview->priv->right,
						 preview->priv->left,
						 preview->priv->vertically,
						 preview->priv->horizontally,
						 preview->priv->cut,
						 &cols_needed, &rows_needed);
		}
	}
	gdk_pixbuf_unref (pixbuf);
}

static gboolean
notify_idle (gpointer data)
{
	EogPreview *preview;

	preview = EOG_PREVIEW (data);

	if (settings_changed (preview))
		update (preview);

	return (FALSE);
}

static void
notify (GConfClient *client, guint cnxn, GConfEntry *entry, gpointer data)
{
	EogPreview *preview;

	preview = EOG_PREVIEW (data);

	gtk_idle_add (notify_idle, preview);
}

static void
remove_notification (EogPreview *preview)
{
	GConfClient	  *client;

	client = preview->priv->client;

	gconf_client_notify_remove (client, preview->priv->notify_orientation);
	gconf_client_notify_remove (client, preview->priv->notify_paper);
	gconf_client_notify_remove (client, preview->priv->notify_right);
	gconf_client_notify_remove (client, preview->priv->notify_left);
	gconf_client_notify_remove (client, preview->priv->notify_top);
	gconf_client_notify_remove (client, preview->priv->notify_bottom);
	gconf_client_notify_remove (client, preview->priv->notify_fit);
	gconf_client_notify_remove (client, preview->priv->notify_adjust);
	gconf_client_notify_remove (client, preview->priv->notify_vertically);
	gconf_client_notify_remove (client, preview->priv->notify_horizontally);
	gconf_client_notify_remove (client, preview->priv->notify_cut);
}

static void
eog_preview_destroy (GtkObject *object)
{
	EogPreview *preview;

	preview = EOG_PREVIEW (object);

	remove_notification (preview);

	g_free (preview->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
eog_preview_init (EogPreview *preview)
{
	preview->priv = g_new0 (EogPreviewPrivate, 1);
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
	static GtkType preview_type = 0;

	if (!preview_type) {
		static const GtkTypeInfo preview_info = {
			"EogPreview",
			sizeof (EogPreview),
			sizeof (EogPreviewClass),
			(GtkClassInitFunc) eog_preview_class_init,
			(GtkObjectInitFunc) eog_preview_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		preview_type = gtk_type_unique (PARENT_TYPE, &preview_info);
	}

	return (preview_type);
}

static void
add_notification (EogPreview *preview)
{
	GConfClient *client;

	client = preview->priv->client;

	preview->priv->notify_orientation = gconf_client_notify_add (
				client, "/apps/eog/viewer/landscape",
				notify, preview, 
				NULL, NULL);
	preview->priv->notify_paper = gconf_client_notify_add (
				client, "/apps/eog/viewer/paper_size",
				notify, preview, NULL, NULL);
	preview->priv->notify_top = gconf_client_notify_add (
				client, "/apps/eog/viewer/top",
				notify, preview, NULL, NULL);
	preview->priv->notify_bottom = gconf_client_notify_add (
				client, "/apps/eog/viewer/bottom",
				notify, preview, NULL, NULL);
	preview->priv->notify_right = gconf_client_notify_add (
				client, "/apps/eog/viewer/right",
				notify, preview, NULL, NULL);
	preview->priv->notify_left = gconf_client_notify_add (
				client, "/apps/eog/viewer/left",
				notify, preview, NULL, NULL);
	preview->priv->notify_adjust = gconf_client_notify_add (
				client, "/apps/eog/viewer/adjust_to",
				notify, preview, NULL, NULL);
	preview->priv->notify_fit = gconf_client_notify_add (
				client, "/apps/eog/viewer/fit_to_page",
				notify, preview, NULL, NULL);
	preview->priv->notify_horizontally = gconf_client_notify_add (
				client, "/apps/eog/viewer/horizontally",
				notify, preview, NULL, NULL);
	preview->priv->notify_vertically = gconf_client_notify_add (
				client, "/apps/eog/viewer/vertically",
				notify, preview, NULL, NULL);
	preview->priv->notify_cut = gconf_client_notify_add (
				client, "/apps/eog/viewer/cut",
				notify, preview, NULL, NULL);
}

GtkWidget*
eog_preview_new (GConfClient *client, EogImageView *image_view)
{
	EogPreview	*preview;
	GtkWidget	*vbox;

	preview = gtk_type_new (EOG_TYPE_PREVIEW);
	gtk_container_set_border_width (GTK_CONTAINER (preview), 8);
	gtk_container_set_resize_mode (GTK_CONTAINER (preview), 
				       GTK_RESIZE_PARENT);
	gtk_box_set_spacing (GTK_BOX (preview), 8);
	gtk_box_set_homogeneous (GTK_BOX (preview), TRUE);

	preview->priv->client = client;
	preview->priv->image_view = image_view;

	settings_changed (preview);

	/* Create the first vbox */ 
	vbox = gtk_vbox_new (TRUE, 8); 
	gtk_widget_show (vbox); 
	gtk_box_pack_start (GTK_BOX (preview), vbox, TRUE, TRUE, 0); 
	gtk_container_set_resize_mode (GTK_CONTAINER (vbox), GTK_RESIZE_PARENT);

	/* Create the first page (0, 0) */
	preview->priv->root = eog_preview_page_new (image_view, 0, 0);
	gtk_widget_show (preview->priv->root);
	gtk_box_pack_start (GTK_BOX (vbox), preview->priv->root, TRUE, TRUE, 0);

	update (preview);

	add_notification (preview);
	
	return (GTK_WIDGET (preview));
}

