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
};

#define SCALE(param) (0.2 * param)

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

void
eog_preview_update (EogPreview *preview, gdouble width, gdouble height, 
		    gdouble bottom, gdouble top, gdouble right, gdouble left, 
		    gboolean vertically, gboolean horizontally, gboolean cut,
		    gboolean fit_to_page, gint adjust_to) 
{
	BonoboPropertyBag *bag;
	BonoboArg	  *arg;
	EogImage	  *image;
	EogPreviewPrivate *priv;
	GtkWidget 	  *page;
	GtkWidget 	  *vbox;
	GdkPixbuf	  *pixbuf = NULL;
	GdkPixbuf	  *pixbuf_orig;
	GdkInterpType	   interp;
	GList	  	  *children;
	GList	  	  *vbox_children;
	gint	  	   cols_needed, rows_needed;
	gint	  	   i, j;
	gint		   image_width, image_height;
	gint		   pixbuf_width, pixbuf_height;

	g_return_if_fail (EOG_IS_PREVIEW (preview));
	priv = preview->priv;

	/* Get the pixbuf */
	image = eog_image_view_get_image (priv->image_view);
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
	if (fit_to_page) {
		gdouble prop_paper, prop_pixbuf;
		gdouble avail_width, avail_height;

		avail_width = width - left - right;
		avail_height = height - top - bottom;
		prop_paper = avail_height / avail_width;
		prop_pixbuf = (gdouble) pixbuf_height / pixbuf_width;

		if (prop_pixbuf > prop_paper) {
			image_width = avail_height / prop_pixbuf;
			image_height = avail_height;
		} else {
			image_width = avail_width;
			image_height = avail_width * prop_pixbuf;
		}
	} else {
		image_width = pixbuf_width * adjust_to / 100;
		image_height = pixbuf_height * adjust_to / 100;
	}

	/* Scale the pixbuf */
	if (((gint) SCALE (image_width) > 0) && 
	    ((gint) SCALE (image_height) > 0))
		pixbuf = gdk_pixbuf_scale_simple (pixbuf_orig, 
						  SCALE (image_width), 
						  SCALE (image_height), interp);
	gdk_pixbuf_unref (pixbuf_orig);

	/* Update the page (0, 0) in order to see how many pages we need */
	eog_preview_page_update (EOG_PREVIEW_PAGE (priv->root), pixbuf,
				 SCALE (width), SCALE (height), SCALE (bottom), 
				 SCALE (top), SCALE (right), SCALE (left),
				 vertically, horizontally, cut,
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
			page = eog_preview_page_new (priv->image_view, i, j);
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
						 SCALE (width), 
						 SCALE (height), 
						 SCALE (bottom), 
						 SCALE (top),
						 SCALE (right), 
						 SCALE (left),
						 vertically, horizontally, cut,
						 &cols_needed, &rows_needed);
		}
	}

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);
}

static void
eog_preview_destroy (GtkObject *object)
{
	EogPreview *preview;

	preview = EOG_PREVIEW (object);

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

GtkWidget*
eog_preview_new (EogImageView *image_view)
{
	EogPreview	*preview;
	GtkWidget	*vbox;

	preview = gtk_type_new (EOG_TYPE_PREVIEW);
	gtk_container_set_border_width (GTK_CONTAINER (preview), 8);
	gtk_container_set_resize_mode (GTK_CONTAINER (preview), 
				       GTK_RESIZE_PARENT);
	gtk_box_set_spacing (GTK_BOX (preview), 8);
	gtk_box_set_homogeneous (GTK_BOX (preview), TRUE);

	preview->priv->image_view = image_view;

	/* Create the first vbox */ 
	vbox = gtk_vbox_new (TRUE, 8); 
	gtk_widget_show (vbox); 
	gtk_box_pack_start (GTK_BOX (preview), vbox, TRUE, TRUE, 0); 
	gtk_container_set_resize_mode (GTK_CONTAINER (vbox), GTK_RESIZE_PARENT);

	/* Create the first page (0, 0) */
	preview->priv->root = eog_preview_page_new (image_view, 0, 0);
	gtk_widget_show (preview->priv->root);
	gtk_box_pack_start (GTK_BOX (vbox), preview->priv->root, TRUE, TRUE, 0);

	return (GTK_WIDGET (preview));
}

