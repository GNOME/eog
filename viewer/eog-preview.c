#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-preview.h>

#include <eog-preview-page.h>
#include <libgnome/gnome-macros.h>

GNOME_CLASS_BOILERPLATE (EogPreview,
			 eog_preview,
			 GtkHBox,
			 GTK_TYPE_HBOX);

struct _EogPreviewPrivate {
	EogImageView *image_view;

	GtkWidget    *root;
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
		    gboolean fit_to_page, gint adjust_to, gdouble overlap_x, 
		    gdouble overlap_y, gboolean overlap) 
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
	arg = bonobo_pbclient_get_value (BONOBO_OBJREF (bag), "interpolation", TC_GNOME_EOG_Interpolation, NULL);
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
	g_object_unref (pixbuf_orig);

	/* Update the page (0, 0) in order to see how many pages we need */
	eog_preview_page_update (EOG_PREVIEW_PAGE (priv->root), pixbuf,
				 SCALE (width), SCALE (height), SCALE (bottom), 
				 SCALE (top), SCALE (right), SCALE (left),
				 vertically, horizontally, cut, 
				 SCALE (overlap_x), SCALE (overlap_y), overlap,
				 &cols_needed, &rows_needed);

	/* Do we need to remove VBoxes? */
	children = gtk_container_get_children (GTK_CONTAINER (preview));
	for (i = g_list_length (children) - 1; i >= cols_needed; i--) {
		vbox = g_list_nth_data (children, i);
		gtk_container_remove (GTK_CONTAINER (preview), vbox);
	}

	/* Do we need to add or remove pages (rows)? */
	children = gtk_container_get_children (GTK_CONTAINER (preview));
	for (i = 0; i < g_list_length (children); i++) {
		vbox = g_list_nth_data (children, i);
		vbox_children = gtk_container_get_children (GTK_CONTAINER (vbox));

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
	children = gtk_container_get_children (GTK_CONTAINER (preview));
	for (i = g_list_length (children); i < cols_needed; i++)
		create_vbox (preview, i, rows_needed);

	/* Update all pages */
	children = gtk_container_get_children (GTK_CONTAINER (preview));
	for (i = 0; i < g_list_length (children); i++) {
		vbox = g_list_nth_data (children, i);
		vbox_children = gtk_container_get_children (GTK_CONTAINER (vbox));
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
						 SCALE (overlap_x), 
						 SCALE (overlap_y), overlap, 
						 &cols_needed, &rows_needed);
		}
	}

	if (pixbuf)
		g_object_unref (pixbuf);
}

static void
eog_preview_dispose (GObject *object)
{
	EogPreview *preview;

	preview = EOG_PREVIEW (object);

	g_free (preview->priv);
	preview->priv = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_preview_instance_init (EogPreview *preview)
{
	preview->priv = g_new0 (EogPreviewPrivate, 1);
}

static void
eog_preview_class_init (EogPreviewClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass *) klass;

	gobject_class->dispose = eog_preview_dispose;
}

GtkWidget *
eog_preview_new (EogImageView *image_view)
{
	GtkWidget  *vbox;
	EogPreview *preview;

	preview = g_object_new (EOG_TYPE_PREVIEW, NULL);

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

	return GTK_WIDGET (preview);
}

