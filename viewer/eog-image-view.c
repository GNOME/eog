/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-view.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@triq.net)
 *
 * Copyright 2000 SuSE GmbH.
 * Copyright 2001-2002 Free Software Foundation
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <glib/gstrfuncs.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>
#include <gconf/gconf-client.h>

#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include <eog-print-setup.h>
#include <eog-image-view.h>
#include <eog-full-screen.h>
#include <image-view.h>
#include <ui-image.h>

#include <preferences.h>

#include <bonobo/bonobo-stream.h>
#include <bonobo/bonobo-ui-util.h>

#ifdef ENABLE_EVOLUTION
#  include "Evolution-Composer.h"
#  include <bonobo/bonobo-stream-memory.h>
#endif
#ifdef ENABLE_GNOCAM
#  include "GnoCam.h"
#endif

struct _EogImageViewPrivate {
	EogImage              *image;

	GConfClient	      *client;
	guint                 interp_type_notify_id;
	guint                 dither_notify_id;
	guint                 check_type_notify_id;
	guint                 check_size_notify_id;

	GtkWidget             *ui_image;
        ImageView             *image_view;

	BonoboPropertyBag     *property_bag;

	BonoboUIComponent     *uic;

	gboolean               zoom_fit;
};

enum {
	PROP_INTERPOLATION,
	PROP_DITHER,
	PROP_CHECK_TYPE,
	PROP_CHECK_SIZE,
	PROP_IMAGE_WIDTH,
	PROP_IMAGE_HEIGHT,
	PROP_WINDOW_TITLE,
	PROP_WINDOW_STATUS
};

enum {
	PROP_CONTROL_TITLE
};

static BonoboControl* property_control_get_cb (BonoboPropertyControl *property_control,
					       int page_number, void *closure);

static GObjectClass *eog_image_view_parent_class;

static GNOME_EOG_Image
impl_GNOME_EOG_ImageView_getImage (PortableServer_Servant servant,
				   CORBA_Environment *ev)
{
	EogImageView *image_view;
	GNOME_EOG_Image image;

	image_view = EOG_IMAGE_VIEW (bonobo_object_from_servant (servant));
	image = BONOBO_OBJREF (image_view->priv->image);

	CORBA_Object_duplicate (image, ev);
	return image;
}


static void
image_set_image_cb (EogImage *eog_image, EogImageView *image_view)
{
	GdkPixbuf *pixbuf;

	g_return_if_fail (eog_image != NULL);
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE (eog_image));
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	pixbuf = eog_image_get_pixbuf (eog_image);
	if (pixbuf) {
		image_view_set_pixbuf (image_view->priv->image_view, pixbuf);
		g_object_unref (pixbuf);
	}
}

static void
listener_Interpolation_cb (BonoboUIComponent           *uic,
			   const char                  *path,
			   Bonobo_UIComponent_EventType type,
			   const char                  *state,
			   gpointer                     user_data)
{
	EogImageView *image_view;
	GdkInterpType interpolation;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "InterpolationNearest")) {
		interpolation = GDK_INTERP_NEAREST;
	}
	else if (!strcmp (path, "InterpolationTiles")) {
		interpolation = GDK_INTERP_TILES;
	}
	else if (!strcmp (path, "InterpolationBilinear")) {
		interpolation = GDK_INTERP_BILINEAR;
	}
	else if (!strcmp (path, "InterpolationHyperbolic")) {
		interpolation = GDK_INTERP_HYPER;
	}
	else {
		g_warning ("Unknown interpolation type `%s'", path);
		return;
	}

	gconf_client_set_int (image_view->priv->client, 
			      GCONF_EOG_VIEW_INTERP_TYPE, 
			      (int) interpolation, NULL);
}

static void
listener_Dither_cb (BonoboUIComponent           *uic,
		    const char                  *path,
		    Bonobo_UIComponent_EventType type,
		    const char                  *state,
		    gpointer                     user_data)
{
	EogImageView *image_view;
	GNOME_EOG_Dither dither;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "DitherNone"))
		dither = GNOME_EOG_DITHER_NONE;
	else if (!strcmp (path, "DitherNormal"))
		dither = GNOME_EOG_DITHER_NORMAL;
	else if (!strcmp (path, "DitherMaximum"))
		dither = GNOME_EOG_DITHER_MAXIMUM;
	else {
		g_warning ("Unknown dither type `%s'", path);
		return;
	}

	gconf_client_set_int (image_view->priv->client, 
			      GCONF_EOG_VIEW_DITHER, 
			      (int) dither, NULL);
}

static void
listener_CheckType_cb (BonoboUIComponent           *uic,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
{
	EogImageView *image_view;
	GNOME_EOG_CheckType check_type;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "CheckTypeDark"))
		check_type = GNOME_EOG_CHECK_TYPE_DARK;
	else if (!strcmp (path, "CheckTypeMidtone"))
		check_type = GNOME_EOG_CHECK_TYPE_MIDTONE;
	else if (!strcmp (path, "CheckTypeLight"))
		check_type = GNOME_EOG_CHECK_TYPE_LIGHT;
	else if (!strcmp (path, "CheckTypeBlack"))
		check_type = GNOME_EOG_CHECK_TYPE_BLACK;
	else if (!strcmp (path, "CheckTypeGray"))
		check_type = GNOME_EOG_CHECK_TYPE_GRAY;
	else if (!strcmp (path, "CheckTypeWhite"))
		check_type = GNOME_EOG_CHECK_TYPE_WHITE;
	else {
		g_warning ("Unknown check type `%s'", path);
		return;
	}

	gconf_client_set_int (image_view->priv->client, 
			      GCONF_EOG_VIEW_CHECK_TYPE, 
			      (int) check_type, NULL);
}

static void
filesel_ok_cb (GtkWidget* ok_button, gpointer user_data)
{
	EogImageView     *image_view;
	GtkWidget        *fsel;
	const gchar      *filename;
	char             *message;
	CORBA_Environment ev;
	Bonobo_Storage    storage = CORBA_OBJECT_NIL;
	Bonobo_Stream     stream = CORBA_OBJECT_NIL;
	gchar            *path_dir = NULL; 
	gchar            *path_base = NULL;
        gchar            *mime_type = NULL;
	gchar            *tmp_dir = NULL;
	GtkWidget        *dialog;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);
	fsel = gtk_widget_get_ancestor (ok_button, GTK_TYPE_FILE_SELECTION);
	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (fsel));

	CORBA_exception_init (&ev);
	
	tmp_dir = g_path_get_dirname (filename);
	path_dir = g_strconcat ("file:", tmp_dir, NULL);
	g_free (tmp_dir);
	storage = bonobo_get_object (path_dir, "IDL:Bonobo/Storage:1.0", &ev);
	if (BONOBO_EX (&ev)) goto on_error;
	g_free (path_dir); path_dir = NULL;

	path_base = g_path_get_basename (filename);
	stream = Bonobo_Storage_openStream (storage, path_base, 
					    Bonobo_Storage_WRITE | Bonobo_Storage_CREATE,
					    &ev);

	if (BONOBO_EX (&ev)) goto on_error;
	g_free (path_base); path_base = NULL;

	mime_type = gnome_vfs_get_mime_type (filename);
	eog_image_save_to_stream (image_view->priv->image,
				  stream, 
				  mime_type, &ev);
	if (BONOBO_EX (&ev)) goto on_error;

	g_free (mime_type);
	bonobo_object_release_unref (stream, &ev);
	bonobo_object_release_unref (storage, &ev);

	CORBA_exception_free (&ev);
	gtk_widget_destroy (fsel);

	return;

 on_error:
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (fsel), GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE,
		_("Could not save image as '%s': %s."), filename, 
		(message = bonobo_exception_get_text (&ev)));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (message);
	g_free (path_dir);
	g_free (path_base);
	g_free (mime_type);

	if (stream != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (stream, &ev);
	if (storage != CORBA_OBJECT_NIL) 
		bonobo_object_release_unref (storage, &ev);

	CORBA_exception_free (&ev);
	gtk_widget_destroy (fsel);
}

static void
filesel_cancel_cb (GtkWidget* cancel_button, gpointer user_data)
{
	GtkWidget *filesel;

	filesel = gtk_widget_get_ancestor (cancel_button, GTK_TYPE_WINDOW);
	gtk_widget_destroy (filesel);
}

static void
verb_FullScreen_cb (BonoboUIComponent *uic, gpointer data, const char *name)
{
	EogImageView *image_view;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (data));

	image_view = EOG_IMAGE_VIEW (data);

	gtk_widget_show (eog_full_screen_new (image_view->priv->image));
}

static void
verb_SaveAs_cb (BonoboUIComponent *uic, gpointer user_data, const char *name)
{
	EogImageView     *image_view;
	GtkFileSelection *filesel;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	filesel = GTK_FILE_SELECTION (gtk_file_selection_new (_("Save As")));
	g_signal_connect (filesel->ok_button, "clicked", 
			  G_CALLBACK (filesel_ok_cb), image_view);
	g_signal_connect (filesel->cancel_button, "clicked", 
			  G_CALLBACK (filesel_cancel_cb), image_view);
	gtk_widget_show (GTK_WIDGET (filesel));
}

#ifdef ENABLE_GNOCAM

typedef struct
{
	EogImageView *image_view;
	GNOME_Camera camera;
	Bonobo_EventSource_ListenerId id;
} ListenerData;

static void
listener_cb (BonoboListener *listener, gchar *event_name, CORBA_any *any, 
	     CORBA_Environment *ev, gpointer data)
{
	ListenerData *listener_data;
	Bonobo_Storage storage;
	Bonobo_Stream stream;
	EogImageView *image_view;
	GNOME_Camera camera;

	if (getenv ("DEBUG_EOG"))
		g_message ("Got event: %s", event_name);

	listener_data = data;
	image_view = listener_data->image_view;
	camera = listener_data->camera;

	bonobo_event_source_client_remove_listener (listener_data->camera,
						    listener_data->id, NULL);
	g_free (listener_data);

	if (!strcmp (event_name, "GNOME/Camera:CaptureImage:Action")) {
		storage = Bonobo_Unknown_queryInterface (camera,
					"IDL:Bonobo/Storage:1.0", ev);
		bonobo_object_release_unref (camera, NULL);
		if (BONOBO_EX (ev)) {
			g_warning ("Could not get storage: %s",
				   bonobo_exception_get_text (ev));
			return;
		}

		stream = Bonobo_Storage_openStream (storage,
					BONOBO_ARG_GET_STRING (any),
					Bonobo_Storage_READ, ev);
		bonobo_object_release_unref (storage, NULL);
		if (BONOBO_EX (ev)) {
			g_warning ("Could not get stream: %s",
				   bonobo_exception_get_text (ev));
			return;
		} 

		eog_image_load_from_stream (image_view->priv->image,
					    stream, ev);
		bonobo_object_release_unref (stream, NULL);
		if (BONOBO_EX (ev)) {
			g_warning ("Could not load image: %s",
				   bonobo_exception_get_text (ev));
			return;
		}
	} else {
		bonobo_object_release_unref (camera, NULL);
	}
}

static void
verb_AcquireFromCamera_cb (BonoboUIComponent *uic, gpointer user_data,
			   const char *name)
{
	EogImageView *image_view;
	CORBA_Environment ev;
	CORBA_Object gnocam;
	GNOME_Camera camera;
	ListenerData *listener_data;

	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));
	
	image_view = EOG_IMAGE_VIEW (user_data);

	CORBA_exception_init (&ev);
	
	gnocam = oaf_activate_from_id ("OAFIID:GNOME_GnoCam", 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to start GnoCam: %s",
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	camera = GNOME_GnoCam_getCamera (gnocam, &ev);
	bonobo_object_release_unref (gnocam, NULL);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to get camera: %s",
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	listener_data = g_new0 (ListenerData, 1);
	listener_data->image_view = image_view;
	listener_data->camera = camera;
	listener_data->id = bonobo_event_source_client_add_listener (camera,
				listener_cb, "GNOME/Camera:CaptureImage",
				&ev, listener_data);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to add listener: %s", 
			   bonobo_exception_get_text (&ev));
		g_free (listener_data);
		bonobo_object_release_unref (camera, NULL);
		CORBA_exception_free (&ev);
		return;
	}

	GNOME_Camera_captureImage (camera, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Could not capture image: %s",
			   bonobo_exception_get_text (&ev));
		bonobo_event_source_client_remove_listener (camera,
							    listener_data->id,
							    NULL);
		g_free (listener_data);
		bonobo_object_release_unref (camera, NULL);
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
}
#endif

#ifdef ENABLE_EVOLUTION
static void
verb_Send_cb (BonoboUIComponent *uic, gpointer user_data, const char *name)
{
	EogImageView *image_view;
	CORBA_Object composer;
	CORBA_Environment ev;
	BonoboStream *stream;
	GNOME_Evolution_Composer_AttachmentData *attachment_data;
	const char *filename;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));
	image_view = EOG_IMAGE_VIEW (user_data);

	CORBA_exception_init (&ev);
	composer = oaf_activate_from_id ("OAFIID:GNOME_Evolution_Mail_Composer",
			                 0, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to start composer: %s",
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	stream = bonobo_stream_mem_create (NULL, 0, FALSE, TRUE);
	eog_image_save_to_stream (image_view->priv->image,
				  BONOBO_OBJREF (stream),
			          "image/png", &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to save image to stream: %s",
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (composer, NULL);
		return;
	}
	
	attachment_data = GNOME_Evolution_Composer_AttachmentData__alloc ();
	attachment_data->_buffer = BONOBO_STREAM_MEM (stream)->buffer;
	attachment_data->_length = BONOBO_STREAM_MEM (stream)->size;
	BONOBO_STREAM_MEM (stream)->buffer = NULL;
	bonobo_object_unref (BONOBO_OBJECT (stream));
	filename = eog_image_get_filename (image_view->priv->image);
	GNOME_Evolution_Composer_attachData (composer, "image/png", filename,
					     filename, FALSE,
					     attachment_data, &ev);
	CORBA_free (attachment_data);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to attach image: %s", 
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (composer, NULL);
		return;
	}

	GNOME_Evolution_Composer_show (composer, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Unable to show composer: %s", 
			   bonobo_exception_get_text (&ev));
		CORBA_exception_free (&ev);
		bonobo_object_release_unref (composer, NULL);
		return;
	}

	CORBA_exception_free (&ev);
}
#endif

/* ***************************************************************************
 * Start of printing related code 
 * ***************************************************************************/

#if 0 
static gint
count_pages (gdouble 	paper_width,
	     gdouble	paper_height,
	     gdouble	top,
	     gdouble	left,
	     gdouble	right,
	     gdouble	bottom,
	     gboolean	fit_to_page,
	     gint	adjust_to,
	     gdouble	overlap_x,
	     gdouble	overlap_y,
	     GdkPixbuf *pixbuf)
{
	gint	adj_width, adj_height;
	gint	image_width, image_height;
	gdouble	avail_width, avail_height;
	gint	rows, cols;

	avail_width = paper_width - left - right - overlap_x;
	avail_height = paper_height - bottom - top - overlap_y;

	if ((avail_width <= 0.0) || (avail_height <= 0.0))
		return (0);

	if (fit_to_page)
		return (1);

	image_width = gdk_pixbuf_get_width (pixbuf);
	image_height = gdk_pixbuf_get_height (pixbuf);

	adj_width = image_width * adjust_to / 100;
	adj_height = image_height * adjust_to / 100;

        for (cols = 1; adj_width > cols * avail_width + overlap_x; cols++);
        for (rows = 1; adj_height > rows * avail_height + overlap_y; rows++);

	return (cols * rows);
}
#endif

static void
print_line (GnomePrintContext *context, 
	    double x1, double y1, double x2, double y2)
{
	gnome_print_moveto (context, x1, y1);
	gnome_print_lineto (context, x2, y2);
	gnome_print_stroke (context);
}

static void
print_cutting_help (GnomePrintContext *context, gdouble width, gdouble height,
		    gdouble x, gdouble y, 
		    gdouble image_width, gdouble image_height)
{
	gdouble x1, y1, x2, y2;

	x1 = x + image_width;
	y1 = y + image_height;
	x2 = x1 + (width - x - image_width) / 3;
	y2 = height - 2 * (height - y - image_height) / 3;

	print_line (context, x, 0.0, x, 2 * y / 3);
	print_line (context, x1, 0.0, x1, 2 * y / 3);
	print_line (context, 0.0, y, 2 * x / 3, y);
	print_line (context, 0.0, y1, 2 * x / 3, y1);
	print_line (context, x2, y1, width, y1);
	print_line (context,  x2, y, width, y);
	print_line (context, x, height , x, y2);
	print_line (context, x1, height, x1, y2);
}

static void
print_overlapping_help (GnomePrintContext *context, gdouble width, 
			gdouble height, gdouble x, gdouble y, 
			gdouble image_width, gdouble image_height, 
			gdouble overlap_x, gdouble overlap_y, gboolean last_x,
			gboolean last_y)
{
	gdouble x1;

	x1 = x + image_width - overlap_x;

	if (!last_x)
		print_line (context, x1, y, x1, y + image_height);
	if (!last_y)
		print_line (context, x, overlap_y, x + image_width, overlap_y);
}

static void
print_page (GnomePrintContext 	*context,
	    gint		 first,
	    gint		 last,
	    gint		*current,
	    gdouble		 width,
	    gdouble 		 height,
	    gboolean		 landscape,
	    gdouble		 top,
	    gdouble		 left,
	    gdouble		 right,
	    gdouble		 bottom,
	    gboolean		 vertically,
	    gboolean		 horizontally,
	    gboolean 		 fit_to_page,
	    gint		 adj,
	    gboolean		 down_right,
	    gboolean		 cut,
	    gdouble		 overlap_x,
	    gdouble 		 overlap_y,
	    gboolean		 overlap,
	    GdkPixbuf 		*pixbuf, 
	    gint 		 col, 
	    gint 		 row)
{
	double     	 matrix [] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	gdouble		 x, y;
	gdouble     	 avail_width, avail_height;
	gdouble    	 leftover_width, leftover_height;
	gdouble     	 image_width, image_height;
	gint		 pixbuf_width, pixbuf_height;
	gint		 cols, rows;
	gboolean   	 go_right = FALSE;
	gboolean   	 go_down = FALSE;
	gboolean	 first_x, last_x;
	gboolean	 first_y, last_y;
	GdkPixbuf 	*pixbuf_to_print = NULL;

	(*current)++;
	if (*current > last)
		return;

	if (*current >= first) {
		gnome_print_beginpage (context, _("EOG Image"));

		/* Move (0,0) to bottom left. */
		if (landscape) { 
			gnome_print_rotate (context, 90.0); 
			gnome_print_translate (context, 0.0, - height); 
		}
	}

	/* How much place do we have got on the paper? */
	avail_width = width - left - right - overlap_x;
	avail_height = height - bottom - top - overlap_y;
	g_return_if_fail (avail_width > 0);
	g_return_if_fail (avail_height > 0);

	/* How big is the pixbuf? */
	pixbuf_width = gdk_pixbuf_get_width (pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (pixbuf);

	/* Calculate the free place on the paper */
	for (cols = 1; pixbuf_width > cols * avail_width + overlap_x; cols++);
	leftover_width = cols * avail_width + overlap_x - pixbuf_width;
	for (rows = 1; pixbuf_height > rows * avail_height + overlap_y; rows++);
	leftover_height = rows * avail_height + overlap_y - pixbuf_height;
	
	first_x = (col == 0);
	first_y = (row == 0);
	last_x = (col == cols - 1);
	last_y = (row == rows - 1);

	/* Width of image? */
	if (first_x && last_x)
		image_width = pixbuf_width;
	else if (last_x) {
		image_width = pixbuf_width - (cols - 1) * avail_width;
		if (horizontally)
			image_width += leftover_width / 2.0;
	} else {
		go_right = TRUE;
		image_width = avail_width + overlap_x;
		if (first_x && horizontally)
			image_width -= leftover_width / 2.0;
	}
	g_return_if_fail ((gint) image_width > 0);

	/* Height of image? */
	if (first_y && last_y)
		image_height = pixbuf_height;
	else if (last_y) {
		image_height = pixbuf_height - (rows - 1) * avail_height;
		if (vertically)
			image_height += leftover_height / 2.0;
	} else {
		go_down = TRUE;
		image_height = avail_height + overlap_y;
		if (first_y && vertically)
			image_height -= leftover_height / 2.0;
	}
	g_return_if_fail ((gint) image_height > 0);

	/* Only do this if we really need this page */
	if (*current >= first) {	
		matrix [0] = image_width;
		matrix [3] = image_height;

		pixbuf_to_print = gdk_pixbuf_new (
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
			x = avail_width * col;
			if (horizontally)
				x -= leftover_width / 2.0;
		}
		g_return_if_fail (x >= 0);
	
		/* Where do we begin to copy (y)? */
		if (first_y)
			y = 0.0;
		else if (last_y)
			y = pixbuf_height - image_height;
		else {
			y = avail_height * row;
			if (vertically)
				y -= leftover_height / 2.0;
		}
		g_return_if_fail (y >= 0);
	
		gdk_pixbuf_copy_area (pixbuf, (gint) x, (gint) y, 
				      (gint) image_width, (gint) image_height, 
				      pixbuf_to_print, 0, 0);
	
		/* Where to put the image (x)? */
		x = left;
		if (horizontally && first_x)
		    	x += leftover_width / 2.0;
		matrix [4] = x;
	
		/* Where to put the image (y)? */
		y = bottom + (avail_height - image_height + overlap_y);
		if (vertically && first_y)
		    	y -= leftover_height / 2.0;
		matrix [5] = y;

		/* Print the image */
		gnome_print_gsave (context);
		gnome_print_concat (context, matrix);

		if (gdk_pixbuf_get_has_alpha (pixbuf_to_print))
			gnome_print_rgbaimage (context, 
					       gdk_pixbuf_get_pixels (pixbuf_to_print),
					       gdk_pixbuf_get_width (pixbuf_to_print),
					       gdk_pixbuf_get_height (pixbuf_to_print),
					       gdk_pixbuf_get_rowstride (pixbuf_to_print));
		else
			gnome_print_rgbimage (context, 
					      gdk_pixbuf_get_pixels (pixbuf_to_print),
					      gdk_pixbuf_get_width (pixbuf_to_print),
					      gdk_pixbuf_get_height (pixbuf_to_print),
					      gdk_pixbuf_get_rowstride (pixbuf_to_print));

		g_object_unref (pixbuf_to_print);
		gnome_print_grestore (context);
		
		/* Helpers? */
		if (cut)
			print_cutting_help (context, width, height, x, y,
					    image_width, image_height);
		if (overlap)
			print_overlapping_help (context, width, height, x, y, 
						image_width, image_height,
						overlap_x, overlap_y, 
						last_x, last_y);

		gnome_print_showpage (context);
	}

	if (down_right) {
		if (go_down)
			print_page (context,
				    first, last, current,
				    width, height, landscape,
				    top, left, right, bottom,
				    vertically, horizontally,
				    fit_to_page, adj, down_right, cut,
				    overlap_x, overlap_y, overlap,
				    pixbuf, col, row + 1);
		else if (go_right)
			print_page (context,
				    first, last, current,
				    width, height, landscape,
				    top, left, right, bottom,
				    vertically, horizontally,
				    fit_to_page, adj, down_right, cut,
				    overlap_x, overlap_y, overlap,
				    pixbuf, col + 1, 0);
		return;
	}

	if (!down_right) {
		if (go_right)
			print_page (context,
				    first, last, current,
				    width, height, landscape,
				    top, left, right, bottom,
				    vertically, horizontally,
				    fit_to_page, adj, down_right, cut,
				    overlap_x, overlap_y, overlap,
				    pixbuf, col + 1, row);
		else if (go_down)
			print_page (context,
				    first, last, current,
				    width, height, landscape,
				    top, left, right, bottom,
				    vertically, horizontally,
				    fit_to_page, adj, down_right, cut,
				    overlap_x, overlap_y, overlap,
				    pixbuf, 0, row + 1);
	}
}

void
eog_image_view_print (EogImageView *image_view, gboolean preview, 
		      const gchar *paper_size, gboolean landscape, 
		      gdouble bottom, gdouble top, gdouble right, gdouble left, 
		      gboolean vertically, gboolean horizontally, 
		      gboolean down_right, gboolean cut, gboolean fit_to_page, 
		      gint adjust_to, gdouble overlap_x, gdouble overlap_y,
		      gboolean overlap)
{
#if 0 
/* FIXME GNOME2: make printing work! */
	GdkPixbuf	  *pixbuf;
	GdkPixbuf	  *pixbuf_orig;
	GdkInterpType	   interp;
	GnomePrintContext *print_context;
	GnomePrintMaster  *print_master;
	GnomePrinter      *printer = NULL;
	const GnomePaper  *paper = NULL;
	int		   first, last, current;
	gint		   pixbuf_width, pixbuf_height;
	gint		   width, height;
	gdouble		   paper_width, paper_height;

	print_master = gnome_print_master_new ();

	/* What paper do we use? */
	paper = gnome_paper_with_name (paper_size);
	gnome_print_master_set_paper (print_master, paper);

	eog_util_paper_size (paper_size, landscape, 
			     &paper_width, &paper_height);

	pixbuf_orig = eog_image_get_pixbuf (image_view->priv->image);

	/* Per default, we print all pages */
	first = 1;
	last = count_pages (paper_width, paper_height, 
			    top, left, right, bottom, 
			    fit_to_page, adjust_to, overlap_x, overlap_y, 
			    pixbuf_orig);

	if (!preview) {
		GnomePrintDialog *gpd;
		gint		  copies;
		gint		  collate;

		gpd = GNOME_PRINT_DIALOG (
			gnome_print_dialog_new (_("Print Image"), 
						GNOME_PRINT_DIALOG_COPIES | 
						GNOME_PRINT_DIALOG_RANGE));
		gnome_dialog_set_default (GNOME_DIALOG (gpd), 
					  GNOME_PRINT_PRINT);
		gnome_print_dialog_construct_range_page (gpd,
					GNOME_PRINT_RANGE_ALL | 
					GNOME_PRINT_RANGE_RANGE,
					1, last , NULL, _("Pages"));

		switch (gnome_dialog_run (GNOME_DIALOG (gpd))) {
		case GNOME_PRINT_PRINT:
			break;
		case GNOME_PRINT_PREVIEW:
			preview = TRUE;
			break;
		case -1:
			gtk_object_unref (GTK_OBJECT (print_master));
			return;
		default:
			gnome_dialog_close (GNOME_DIALOG (gpd));
			gtk_object_unref (GTK_OBJECT (print_master));
			return;
		}

		gnome_print_dialog_get_copies (gpd, &copies, &collate);
		gnome_print_master_set_copies (print_master, copies, collate);

		printer = gnome_print_dialog_get_printer (gpd);
		gnome_print_master_set_printer (print_master, printer);

		gnome_print_dialog_get_range_page (gpd, &first, &last);

		gnome_dialog_close (GNOME_DIALOG (gpd));
	}

	print_context = gnome_print_master_get_context (print_master);

	current = 0;

	/* Get the interpolation type */
	interp = image_view_get_interp_type (image_view->priv->image_view);
	
	/* Get the size of the pixbuf */ 
	pixbuf_width = gdk_pixbuf_get_width (pixbuf_orig); 
	pixbuf_height = gdk_pixbuf_get_height (pixbuf_orig);

	/* Calculate width and height of image */
	if (fit_to_page) {
		gdouble prop_paper, prop_pixbuf;
		gdouble avail_width, avail_height;

		avail_width = paper_width - right - left;
		avail_height = paper_height - top - bottom;

		prop_paper = avail_height / avail_width;
		prop_pixbuf = (gdouble) pixbuf_height / pixbuf_width;

		if (prop_pixbuf > prop_paper) { 
			width = avail_height / prop_pixbuf; 
			height = avail_height; 
		} else { 
			width = avail_width; 
			height = avail_width * prop_pixbuf; 
		}
	} else {
		width = pixbuf_width * adjust_to / 100;
		height = pixbuf_height * adjust_to / 100;
	}
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);

	/* Scale the pixbuf */
	pixbuf = gdk_pixbuf_scale_simple (pixbuf_orig, width, height, interp);
	g_object_unref (pixbuf_orig);

	/* Print it! */
	print_page (print_context, 
		    first, last, &current,
		    paper_width, paper_height, 
		    landscape,
		    top, left, right, bottom,
		    vertically, horizontally,
		    fit_to_page, adjust_to,
		    down_right, cut, overlap_x, overlap_y, overlap,
		    pixbuf, 0, 0);
	g_object_unref (pixbuf);

	gnome_print_context_close (print_context);
	gnome_print_master_close (print_master);

	if (preview) {
		GnomePrintMasterPreview *preview;

		preview = gnome_print_master_preview_new_with_orientation (
			print_master, _("Print Preview"), landscape);
		gtk_widget_show (GTK_WIDGET (preview));
	} else {
		int result = gnome_print_master_print (print_master);

		if (result == -1)
			g_warning (_("Printing of image failed"));
	}

	gtk_object_unref (GTK_OBJECT (print_master));
#endif /* FIXME GNOME2: make printing work */
}

static void
verb_PrintPreview_cb (BonoboUIComponent *uic, 
		      gpointer 		 user_data, 
		      const char 	*name)
{
	EogImageView 	*image_view;
	gchar		*paper_size;
	gboolean	 landscape, down_right;
	gdouble		 bottom, top, right, left, overlap_x, overlap_y;
	gboolean	 vertically, horizontally, cut, fit_to_page, overlap;
	gint		 adjust_to, unit;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	eog_util_load_print_settings (image_view->priv->client,
				      &paper_size, &top, &bottom, &left,
				      &right, &landscape, &cut, &horizontally, 
				      &vertically, &down_right, &fit_to_page, 
				      &adjust_to, &unit, &overlap_x, 
				      &overlap_y, &overlap);
	eog_image_view_print (image_view, TRUE, paper_size, landscape,
			      bottom, top, right, left, vertically, 
			      horizontally, down_right, cut, fit_to_page, 
			      adjust_to, overlap_x, overlap_y, overlap);
	g_free (paper_size);
}

static void
verb_Print_cb (BonoboUIComponent *uic, gpointer user_data, const char *name)
{
	EogImageView 	*image_view;
	gchar	 	*paper_size;
	gboolean	 landscape, down_right;
	gdouble		 bottom, top, right, left, overlap_x, overlap_y;
	gboolean	 vertically, horizontally, cut, fit_to_page, overlap;
	gint		 adjust_to, unit;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);

	eog_util_load_print_settings (image_view->priv->client, 
				      &paper_size, &top, &bottom, &left, 
				      &right, &landscape, &cut, &horizontally, 
				      &vertically, &down_right, &fit_to_page, 
				      &adjust_to, &unit, &overlap_x,
				      &overlap_y, &overlap);
	eog_image_view_print (image_view, FALSE, paper_size, landscape, 
			      bottom, top, right, left, vertically, 
			      horizontally, down_right, cut, fit_to_page, 
			      adjust_to, overlap_x, overlap_y, overlap);
	g_free (paper_size);
}

static void
verb_PrintSetup_cb (BonoboUIComponent *uic, gpointer user_data, 
		    const char *name)
{
	EogImageView 	*image_view;
	GtkWidget	*print_setup;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);
	print_setup = eog_print_setup_new (image_view);
	gtk_widget_show (print_setup);
}

static void
listener_CheckSize_cb (BonoboUIComponent           *uic,
		       const char                  *path,
		       Bonobo_UIComponent_EventType type,
		       const char                  *state,
		       gpointer                     user_data)
{
	EogImageView *image_view;
	GNOME_EOG_CheckSize check_size;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_view = EOG_IMAGE_VIEW (user_data);

	if (!strcmp (path, "CheckSizeSmall"))
		check_size = GNOME_EOG_CHECK_SIZE_SMALL;
	else if (!strcmp (path, "CheckSizeMedium"))
		check_size = GNOME_EOG_CHECK_SIZE_MEDIUM;
	else if (!strcmp (path, "CheckSizeLarge"))
		check_size = GNOME_EOG_CHECK_SIZE_LARGE;
	else {
		g_warning ("Unknown check size `%s'", path);
		return;
	}

	gconf_client_set_int (image_view->priv->client, 
			      GCONF_EOG_VIEW_CHECK_SIZE, 
			      (int) check_size, NULL);
}


static const gchar* ui_id_strings_interp_type [] = {
	"InterpolationNearest",
	"InterpolationTiles",
	"InterpolationBilinear",
	"InterpolationHyperbolic"
};

static const gchar* ui_id_strings_dither [] = {
	"DitherNone",
	"DitherNormal",
	"DitherMaximum"
};

static const gchar* ui_id_strings_check_type [] = {
	"CheckTypeDark",
	"CheckTypeMidtone",
	"CheckTypeLight",
	"CheckTypeBlack",
	"CheckTypeGray",
	"CheckTypeWhite"
};

static const gchar* ui_id_strings_check_size [] = {
	"CheckSizeSmall",
	"CheckSizeMedium",
	"CheckSizeLarge"
};

static void
set_ui_group_item (EogImageView *image_view, const gchar *group_cmd)
{
	gchar *path;
	gchar *value;
	
	path = g_strconcat ("/commands/", group_cmd, NULL);

	value = bonobo_ui_component_get_prop (image_view->priv->uic,
					      path, "state", NULL);
	if (value == NULL || !g_ascii_strcasecmp (value, "0")) {
		bonobo_ui_component_set_prop (image_view->priv->uic, 
					      path, "state", "1", NULL);
	}

	g_free (value);
	g_free (path);
}


#define EVOLUTION_MENU "<menuitem name=\"Send\" _label=\"Send\" pixtype=\"stock\" pixname=\"New Mail\" verb=\"\"/>"
#define GNOCAM_MENU "<menuitem name=\"AcquireFromCamera\" _label=\"Acquire from camera\" verb=\"\"/>"

static void
eog_image_view_create_ui (EogImageView *image_view)
{
	int value;
	int i;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	/* Set up the UI from an XML file. */
        bonobo_ui_util_set_ui (image_view->priv->uic, DATADIR,
			       "eog-image-view-ui.xml", "EogImageView", NULL);

#ifdef ENABLE_EVOLUTION
	bonobo_ui_component_set_translate (image_view->priv->uic,
					   "/menu/File/FileOperations", 
					   EVOLUTION_MENU, NULL);
#endif
#ifdef ENABLE_GNOCAM
	bonobo_ui_component_set_translate (image_view->priv->uic,
					   "/menu/File/FileOperations",
					   GNOCAM_MENU, NULL);
#endif

	value = gconf_client_get_int (image_view->priv->client, 
				      GCONF_EOG_VIEW_INTERP_TYPE, 
				      NULL);
	set_ui_group_item (image_view, ui_id_strings_interp_type [value]);
	for (i = 0; i < 4; i++) {
		bonobo_ui_component_add_listener (image_view->priv->uic, ui_id_strings_interp_type [i],
						  listener_Interpolation_cb, image_view);
	}

	value = gconf_client_get_int (image_view->priv->client, 
				      GCONF_EOG_VIEW_DITHER, 
				       NULL);
	set_ui_group_item (image_view, ui_id_strings_dither [value]);
	for (i = 0; i < 3; i++) {
		bonobo_ui_component_add_listener (image_view->priv->uic, ui_id_strings_dither [i],
						  listener_Dither_cb, image_view);
	}

	value = gconf_client_get_int (image_view->priv->client, 
				      GCONF_EOG_VIEW_CHECK_TYPE, 
				      NULL);
	set_ui_group_item (image_view, ui_id_strings_check_type [value]);
	for (i = 0; i < 6; i++) {
		bonobo_ui_component_add_listener (image_view->priv->uic, ui_id_strings_check_type [i],
						  listener_CheckType_cb, image_view);
	}

	value = gconf_client_get_int (image_view->priv->client, 
				      GCONF_EOG_VIEW_CHECK_SIZE, 
				      NULL);
	set_ui_group_item (image_view, ui_id_strings_check_size [value]);
	for (i = 0; i < 3; i++) {
		bonobo_ui_component_add_listener (image_view->priv->uic, ui_id_strings_check_size [i],
						  listener_CheckSize_cb, image_view);
	}

	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckSizeSmall",
					  listener_CheckSize_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckSizeMedium",
					  listener_CheckSize_cb, image_view);
	bonobo_ui_component_add_listener (image_view->priv->uic, "CheckSizeLarge",
					  listener_CheckSize_cb, image_view);

	bonobo_ui_component_add_verb (image_view->priv->uic, "SaveAs", 
				      verb_SaveAs_cb, image_view);
	bonobo_ui_component_add_verb (image_view->priv->uic, "FullScreen", 
			              verb_FullScreen_cb, image_view);
#ifdef ENABLE_EVOLUTION
	bonobo_ui_component_add_verb (image_view->priv->uic, "Send",
				      verb_Send_cb, image_view);
#endif
#ifdef ENABLE_GNOCAM
	bonobo_ui_component_add_verb (image_view->priv->uic, "AcquireFromCamera",
				      verb_AcquireFromCamera_cb, image_view);
#endif
	bonobo_ui_component_add_verb (image_view->priv->uic, "PrintSetup", 
				      verb_PrintSetup_cb, image_view);
	bonobo_ui_component_add_verb (image_view->priv->uic, "PrintPreview",
				      verb_PrintPreview_cb, image_view);
	bonobo_ui_component_add_verb (image_view->priv->uic, "Print",
				      verb_Print_cb, image_view);
}

/* ***************************************************************************
 * Start of property-bag related code
 * ***************************************************************************/

static void
eog_image_view_get_prop (BonoboPropertyBag *bag,
			 BonoboArg         *arg,
			 guint              arg_id,
			 CORBA_Environment *ev,
			 gpointer           user_data)
{
	EogImageView *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);
	priv = image_view->priv;

	switch (arg_id) {
	case PROP_INTERPOLATION: {
		GdkInterpType interp_type;
		GNOME_EOG_Interpolation eog_interp;

		g_assert (arg->_type == TC_GNOME_EOG_Interpolation);

		interp_type = (GdkInterpType) gconf_client_get_int (priv->client, 
								    GCONF_EOG_VIEW_INTERP_TYPE, 
								    NULL);
		switch (interp_type) {
		case GDK_INTERP_NEAREST:
			eog_interp = GNOME_EOG_INTERPOLATION_NEAREST;
		case GDK_INTERP_TILES:
			eog_interp = GNOME_EOG_INTERPOLATION_TILES;
		case GDK_INTERP_BILINEAR:
			eog_interp = GNOME_EOG_INTERPOLATION_BILINEAR;
		case GDK_INTERP_HYPER:
			eog_interp = GNOME_EOG_INTERPOLATION_HYPERBOLIC;
		default:
			eog_interp = GNOME_EOG_INTERPOLATION_NEAREST;
		}

		* (GNOME_EOG_Interpolation *) arg->_value = eog_interp;
		break;
	}
	case PROP_DITHER: {
		GdkRgbDither dither;
		GNOME_EOG_Dither eog_dither;

		g_assert (arg->_type == TC_GNOME_EOG_Dither);

		dither = (GdkRgbDither) gconf_client_get_int (priv->client,
							      GCONF_EOG_VIEW_DITHER,
							      NULL);
		switch (dither) {
		case GDK_RGB_DITHER_NONE:
			eog_dither = GNOME_EOG_DITHER_NONE;
		case GDK_RGB_DITHER_NORMAL:
			eog_dither = GNOME_EOG_DITHER_NORMAL;
		case GDK_RGB_DITHER_MAX:
			eog_dither = GNOME_EOG_DITHER_MAXIMUM;
		default:
			eog_dither = GNOME_EOG_DITHER_NONE;
		}

		* (GNOME_EOG_Dither *) arg->_value = eog_dither;
		break;
	}
	case PROP_CHECK_TYPE: {
		CheckType check_type;
		GNOME_EOG_CheckType eog_check_type;

		g_assert (arg->_type == TC_GNOME_EOG_CheckType);

		check_type = (CheckType) gconf_client_get_int (priv->client,
							       GCONF_EOG_VIEW_CHECK_TYPE,
							       NULL);
		switch (check_type) {
		case CHECK_TYPE_DARK:
			eog_check_type = GNOME_EOG_CHECK_TYPE_DARK;
		case CHECK_TYPE_MIDTONE:
			eog_check_type = GNOME_EOG_CHECK_TYPE_MIDTONE;
		case CHECK_TYPE_LIGHT:
			eog_check_type = GNOME_EOG_CHECK_TYPE_LIGHT;
		case CHECK_TYPE_BLACK:
			eog_check_type = GNOME_EOG_CHECK_TYPE_BLACK;
		case CHECK_TYPE_GRAY:
			eog_check_type = GNOME_EOG_CHECK_TYPE_GRAY;
		case CHECK_TYPE_WHITE:
			eog_check_type = GNOME_EOG_CHECK_TYPE_WHITE;
		default:
			eog_check_type = GNOME_EOG_CHECK_TYPE_DARK;
		}

		* (GNOME_EOG_CheckType *) arg->_value = eog_check_type;
		break;
	}
	case PROP_CHECK_SIZE: {
		CheckSize check_size;
		GNOME_EOG_CheckSize eog_check_size;

		g_assert (arg->_type == TC_GNOME_EOG_CheckSize);

		check_size = (CheckSize) gconf_client_get_int (priv->client,
							       GCONF_EOG_VIEW_CHECK_SIZE,
							       NULL);
		switch (check_size) {
		case CHECK_SIZE_SMALL:
			eog_check_size = GNOME_EOG_CHECK_SIZE_SMALL;
		case CHECK_SIZE_MEDIUM:
			eog_check_size = GNOME_EOG_CHECK_SIZE_MEDIUM;
		case CHECK_SIZE_LARGE:
			eog_check_size = GNOME_EOG_CHECK_SIZE_LARGE;
		default:
			eog_check_size = GNOME_EOG_CHECK_SIZE_SMALL;
		}

		* (GNOME_EOG_CheckSize *) arg->_value = eog_check_size;
		break;
	}
	case PROP_IMAGE_WIDTH: {
		GdkPixbuf *pixbuf;

		g_assert (arg->_type == BONOBO_ARG_INT);

		pixbuf = image_view_get_pixbuf (priv->image_view);
		if (pixbuf) {
			BONOBO_ARG_SET_INT (arg, gdk_pixbuf_get_width (pixbuf));
			g_object_unref (pixbuf);
		} else
			BONOBO_ARG_SET_INT (arg, 0);
		break;
	}
	case PROP_IMAGE_HEIGHT: {
		GdkPixbuf *pixbuf;

		g_assert (arg->_type == BONOBO_ARG_INT);

		pixbuf = image_view_get_pixbuf (priv->image_view);
		if (pixbuf) {
			BONOBO_ARG_SET_INT (arg, gdk_pixbuf_get_height (pixbuf));
			g_object_unref (pixbuf);
		} else
			BONOBO_ARG_SET_INT (arg, 0);
		break;
	}
	case PROP_WINDOW_TITLE: {
		const gchar *filename;

		g_assert (arg->_type == BONOBO_ARG_STRING);
		
		filename = eog_image_get_filename (priv->image);
		if (filename)
			BONOBO_ARG_SET_STRING (arg, filename);
		else 
			BONOBO_ARG_SET_STRING (arg, "");
		break;
	}
	case PROP_WINDOW_STATUS: {
		gchar *text = NULL;
		double zoomx, zoomy;
		gint zoom;
		GdkPixbuf *pixbuf;

		g_assert (arg->_type == BONOBO_ARG_STRING);
		
		pixbuf = image_view_get_pixbuf (priv->image_view);
		image_view_get_zoom (priv->image_view, &zoomx, &zoomy);
		zoom = 100 * sqrt (zoomx * zoomy);
		
		text = g_new0 (gchar, 40);
		if (pixbuf) {
			g_snprintf (text, 39, "%i x %i pixels    %i%%", 
				    gdk_pixbuf_get_width (pixbuf),
				    gdk_pixbuf_get_height (pixbuf),
				    zoom);
			g_object_unref (pixbuf);
		} else {
			g_snprintf (text, 39, "%i%%", zoom); 
		}
		BONOBO_ARG_SET_STRING (arg, text);
		g_free (text);
		break;
	}
	default:
		g_assert_not_reached ();
	}
}

static void
eog_image_view_set_prop (BonoboPropertyBag *bag,
			 const BonoboArg   *arg,
			 guint              arg_id,
			 CORBA_Environment *ev,
			 gpointer           user_data)
{
	EogImageView        *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (user_data));

	image_view = EOG_IMAGE_VIEW (user_data);
	priv = image_view->priv;

	switch (arg_id) {
	case PROP_INTERPOLATION: {
		GdkInterpType interp_type = GDK_INTERP_NEAREST;

		g_assert (arg->_type == TC_GNOME_EOG_Interpolation);
		switch (* (GNOME_EOG_Interpolation *) arg->_value) {
		case GNOME_EOG_INTERPOLATION_NEAREST:
			interp_type = GDK_INTERP_NEAREST;
			break;
		case GNOME_EOG_INTERPOLATION_TILES:
			interp_type = GDK_INTERP_TILES;
			break;
		case GNOME_EOG_INTERPOLATION_BILINEAR:
			interp_type = GDK_INTERP_BILINEAR;
			break;
		case GNOME_EOG_INTERPOLATION_HYPERBOLIC:
			interp_type = GDK_INTERP_HYPER;
			break;
		default:
			g_assert_not_reached ();
		}

		gconf_client_set_int (priv->client, 
				      GCONF_EOG_VIEW_INTERP_TYPE,
				      (int) interp_type, 
				      NULL);
		break;
	}
	case PROP_DITHER: {
		GdkRgbDither dither = GDK_RGB_DITHER_NONE;

		g_assert (arg->_type == TC_GNOME_EOG_Dither);

		switch (* (GNOME_EOG_Dither *) arg->_value) {
		case GNOME_EOG_DITHER_NONE:
			dither = GDK_RGB_DITHER_NONE;
			break;
		case GNOME_EOG_DITHER_NORMAL:
			dither = GDK_RGB_DITHER_NORMAL;
			break;
		case GNOME_EOG_DITHER_MAXIMUM:
			dither = GDK_RGB_DITHER_MAX;
			break;
		default:
			g_assert_not_reached ();
		}

		gconf_client_set_int (priv->client, 
				      GCONF_EOG_VIEW_DITHER,
				      (int) dither, 
				      NULL);
		break;
	}
	case PROP_CHECK_TYPE: {
		CheckType check_type = CHECK_TYPE_GRAY;

		g_assert (arg->_type == TC_GNOME_EOG_CheckType);

		switch (* (GNOME_EOG_CheckType *) arg->_value) {
		case GNOME_EOG_CHECK_TYPE_DARK:
			check_type = CHECK_TYPE_DARK;
			break;
		case GNOME_EOG_CHECK_TYPE_MIDTONE:
			check_type = CHECK_TYPE_MIDTONE;
			break;
		case GNOME_EOG_CHECK_TYPE_LIGHT:
			check_type = CHECK_TYPE_LIGHT;
			break;
		case GNOME_EOG_CHECK_TYPE_BLACK:
			check_type = CHECK_TYPE_BLACK;
			break;
		case GNOME_EOG_CHECK_TYPE_GRAY:
			check_type = CHECK_TYPE_GRAY;
			break;
		case GNOME_EOG_CHECK_TYPE_WHITE:
			check_type = CHECK_TYPE_WHITE;
			break;
		default:
			g_assert_not_reached ();
		}

		gconf_client_set_int (priv->client, 
				      GCONF_EOG_VIEW_CHECK_TYPE,
				      (int) check_type, 
				      NULL);
		break;
	}
	case PROP_CHECK_SIZE: {
		CheckSize check_size = GNOME_EOG_CHECK_SIZE_MEDIUM;

		g_assert (arg->_type == TC_GNOME_EOG_CheckSize);

		switch (* (GNOME_EOG_CheckSize *) arg->_value) {
		case GNOME_EOG_CHECK_SIZE_SMALL:
			check_size = CHECK_SIZE_SMALL;
			break;
		case GNOME_EOG_CHECK_SIZE_MEDIUM:
			check_size = CHECK_SIZE_MEDIUM;
			break;
		case GNOME_EOG_CHECK_SIZE_LARGE:
			check_size = CHECK_SIZE_LARGE;
			break;
		default:
			g_assert_not_reached ();
		}

		gconf_client_set_int (priv->client, 
				      GCONF_EOG_VIEW_CHECK_SIZE,
				      (int) check_size, 
				      NULL);
		break;
	}
	default:
		g_assert_not_reached ();
	}
}

EogImage *
eog_image_view_get_image (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->image));
	return image_view->priv->image;
}

BonoboPropertyBag *
eog_image_view_get_property_bag (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	return image_view->priv->property_bag;
}

BonoboPropertyControl *
eog_image_view_get_property_control (EogImageView *image_view)
{
	BonoboEventSource *es;

	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	es = bonobo_event_source_new ();

	return bonobo_property_control_new_full (property_control_get_cb, 
						 1, es, image_view);
}

void
eog_image_view_set_ui_container (EogImageView      *image_view,
				 Bonobo_UIContainer ui_container)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	if (getenv ("DEBUG_EOG"))
		g_message ("Setting ui container for EogImageView...");

	bonobo_ui_component_set_container (image_view->priv->uic, ui_container, NULL);

	eog_image_view_create_ui (image_view);
}

void
eog_image_view_unset_ui_container (EogImageView *image_view)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	if (getenv ("DEBUG_EOG"))
		g_message ("Unsetting ui container for EogImageView...");

	bonobo_ui_component_unset_container (image_view->priv->uic, NULL);
}

GtkWidget *
eog_image_view_get_widget (EogImageView *image_view)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	gtk_widget_ref (image_view->priv->ui_image);

	return image_view->priv->ui_image;
}

void
eog_image_view_get_zoom_factor (EogImageView *image_view, double *zoomx, double *zoomy)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	image_view_get_zoom (image_view->priv->image_view, zoomx, zoomy);
}

void
eog_image_view_set_zoom_factor (EogImageView *image_view,
				double        zoom_factor)
{
	ImageView *view;

	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));
	g_return_if_fail (zoom_factor > 0.0);

	view = image_view->priv->image_view;

	image_view_set_zoom (view, zoom_factor, zoom_factor);
}

void
eog_image_view_set_zoom (EogImageView *image_view,
			 double        zoomx,
			 double        zoomy)
			 
{
	g_return_if_fail (zoomx > 0.0);
	g_return_if_fail (zoomy > 0.0);
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	image_view_set_zoom (image_view->priv->image_view, zoomx, zoomy);
}

void
eog_image_view_zoom_to_fit (EogImageView *image_view,
			    gboolean      keep_aspect_ratio)
{
	g_return_if_fail (image_view != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (image_view));

	ui_image_zoom_fit (UI_IMAGE (image_view->priv->ui_image));
}

static void
image_view_zoom_changed_cb (ImageView *image_view, gpointer data)
{
	BonoboArg *arg;
	EogImageView *view;
	
	view = EOG_IMAGE_VIEW (data);
	
	arg = bonobo_arg_new (BONOBO_ARG_STRING);
	eog_image_view_get_prop (NULL, arg, PROP_WINDOW_STATUS, NULL, view);
		
	bonobo_event_source_notify_listeners (view->priv->property_bag->es,
					      "window/status",
					      arg, NULL);
	bonobo_arg_release (arg);
}

GConfClient* 
eog_image_view_get_client (EogImageView *image_view)
{
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);

	return image_view->priv->client;
}


/* ***************************************************************************
 * Constructor, destructor, etc.
 * ***************************************************************************/

static void
eog_image_view_destroy (BonoboObject *object)
{
	EogImageView *image_view;
	EogImageViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (object));

	if (getenv ("DEBUG_EOG"))
		g_message ("Destroying EogImageView...");

	image_view = EOG_IMAGE_VIEW (object);
	priv = image_view->priv;

	gconf_client_notify_remove (priv->client, priv->interp_type_notify_id);
	gconf_client_notify_remove (priv->client, priv->dither_notify_id);
	gconf_client_notify_remove (priv->client, priv->check_type_notify_id);
	gconf_client_notify_remove (priv->client, priv->check_size_notify_id);
	g_object_unref (G_OBJECT (priv->client));

	bonobo_object_unref (BONOBO_OBJECT (priv->property_bag));
	bonobo_object_unref (BONOBO_OBJECT (priv->image));
	bonobo_object_unref (BONOBO_OBJECT (priv->uic));
	
	if (getenv ("DEBUG_EOG"))
		g_message ("EogImageView destroyed.");

	if (BONOBO_OBJECT_CLASS (eog_image_view_parent_class)->destroy)
		BONOBO_OBJECT_CLASS (eog_image_view_parent_class)->destroy (object);
}

static void
eog_image_view_finalize (GObject *object)
{
	EogImageView *image_view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEW (object));

	image_view = EOG_IMAGE_VIEW (object);

	g_free (image_view->priv);

	if (G_OBJECT_CLASS (eog_image_view_parent_class)->finalize)
		G_OBJECT_CLASS (eog_image_view_parent_class)->finalize (object);
}

static void
eog_image_view_class_init (EogImageViewClass *klass)
{
	BonoboObjectClass *bonobo_object_class = (BonoboObjectClass *)klass;
	GObjectClass *gobject_class = (GObjectClass *)klass;

	POA_GNOME_EOG_ImageView__epv *epv;

	eog_image_view_parent_class = g_type_class_peek_parent (klass);

	bonobo_object_class->destroy = eog_image_view_destroy;
	gobject_class->finalize = eog_image_view_finalize;

	epv = &klass->epv;

	epv->getImage = impl_GNOME_EOG_ImageView_getImage;
}

static void
eog_image_view_init (EogImageView *image_view)
{
	image_view->priv = g_new0 (EogImageViewPrivate, 1);
}

BONOBO_TYPE_FUNC_FULL (EogImageView, 
		       GNOME_EOG_ImageView,
		       BONOBO_TYPE_OBJECT,
		       eog_image_view);

static void
property_control_get_prop (BonoboPropertyBag *bag,
			   BonoboArg         *arg,
			   guint              arg_id,
			   CORBA_Environment *ev,
			   gpointer           user_data)
{
	switch (arg_id) {
	case PROP_CONTROL_TITLE:
		g_assert (arg->_type == BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, _("Display"));
		break;
	default:
		g_assert_not_reached ();
	}
}

static BonoboControl *
property_control_get_cb (BonoboPropertyControl *property_control,
			 int page_number, void *closure)
{
	EogImageView *image_view;
	GtkWidget *container;
	BonoboControl *control;
	BonoboPropertyBag *property_bag;

	g_return_val_if_fail (closure != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (closure), NULL);
	g_return_val_if_fail (page_number == 0, NULL);

	image_view = EOG_IMAGE_VIEW (closure);

	container = eog_create_preferences_page (image_view, page_number);

	gtk_widget_show_all (container);

	control = bonobo_control_new (container);

	/* Property Bag */
	property_bag = bonobo_property_bag_new (property_control_get_prop,
						NULL, control);

	bonobo_property_bag_add (property_bag, "bonobo:title",
				 PROP_CONTROL_TITLE, BONOBO_ARG_STRING,
				 NULL, NULL, BONOBO_PROPERTY_READABLE);

	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (property_bag));

	return control;
}

static void
interp_type_changed_cb (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	EogImageView *view;
	GdkInterpType interpolation;

	view = EOG_IMAGE_VIEW (user_data);
	
	if (entry->value != NULL && entry->value->type == GCONF_VALUE_INT) {
		interpolation = (GdkInterpType) gconf_value_get_int (entry->value);
	}
	else {
		interpolation = GDK_INTERP_BILINEAR;
	}

	image_view_set_interp_type (view->priv->image_view, interpolation);
	bonobo_event_source_notify_listeners (view->priv->property_bag->es,
					      "interpolation", NULL, NULL);
	set_ui_group_item (view, ui_id_strings_interp_type [interpolation]);
}

static void
dither_changed_cb (GConfClient *client,
		   guint        cnxn_id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
	EogImageView *view;
	GdkRgbDither dither;

	view = EOG_IMAGE_VIEW (user_data);
	
	if (entry->value != NULL && entry->value->type == GCONF_VALUE_INT) {
		dither = (GdkRgbDither) gconf_value_get_int (entry->value);
	} 
	else {
		dither = GDK_RGB_DITHER_NONE;
	}

	image_view_set_dither (view->priv->image_view, dither);
	bonobo_event_source_notify_listeners (view->priv->property_bag->es,
					      "dither", NULL, NULL);
	set_ui_group_item (view, ui_id_strings_dither [dither]);
}

static void
check_type_changed_cb (GConfClient *client,
		       guint        cnxn_id,
		       GConfEntry  *entry,
		       gpointer     user_data)
{
	EogImageView *view;
	CheckType check_type;

	view = EOG_IMAGE_VIEW (user_data);
	
	if (entry->value != NULL && entry->value->type == GCONF_VALUE_INT) {
		check_type = (CheckType) gconf_value_get_int (entry->value);
	}
	else {
		check_type = CHECK_TYPE_MIDTONE;
	}

	image_view_set_check_type (view->priv->image_view, check_type);
	bonobo_event_source_notify_listeners (view->priv->property_bag->es,
					      "check_type", NULL, NULL);
	set_ui_group_item (view, ui_id_strings_check_type [check_type]);
}

static void
check_size_changed_cb (GConfClient *client,
		       guint        cnxn_id,
		       GConfEntry  *entry,
		       gpointer     user_data)
{
	EogImageView *view;
	CheckSize check_size;

	view = EOG_IMAGE_VIEW (user_data);
	
	if (entry->value != NULL && entry->value->type == GCONF_VALUE_INT) {
		check_size = (CheckSize) gconf_value_get_int (entry->value);
	}
	else {
		check_size = CHECK_SIZE_MEDIUM;
	}

	image_view_set_check_size (view->priv->image_view, check_size);
	bonobo_event_source_notify_listeners (view->priv->property_bag->es,
					      "check_size", NULL, NULL);
	set_ui_group_item (view, ui_id_strings_check_size [check_size]);
}


EogImageView *
eog_image_view_construct (EogImageView       *image_view,
			  EogImage           *image,
			  gboolean            zoom_fit)
{
	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	/* setup gconf stuff */
	if (!gconf_is_initialized ())
		gconf_init (0, NULL, NULL);	
	image_view->priv->client = gconf_client_get_default ();
	gconf_client_add_dir (image_view->priv->client,
			      GCONF_EOG_VIEW_DIR,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);
	
	image_view->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image_view->priv->image));
	image_view->priv->zoom_fit = zoom_fit;

	g_signal_connect (G_OBJECT (image), "set_image",
			  (GCallback) image_set_image_cb,
			  image_view);

	image_view->priv->ui_image = ui_image_new ();
	gtk_widget_show (image_view->priv->ui_image);

	image_view->priv->image_view = 
		IMAGE_VIEW (ui_image_get_image_view (UI_IMAGE (image_view->priv->ui_image)));
	g_signal_connect (G_OBJECT (image_view->priv->image_view), 
			  "zoom_changed", 
			  (GCallback) image_view_zoom_changed_cb, 
			  image_view);

	/* get preference values from gconf and add listeners */

	image_view_set_interp_type (image_view->priv->image_view,
			            (GdkInterpType) gconf_client_get_int (image_view->priv->client,
									  GCONF_EOG_VIEW_INTERP_TYPE,
									  NULL));
	image_view_set_dither (image_view->priv->image_view,
			       (GdkRgbDither) gconf_client_get_int (image_view->priv->client,
								    GCONF_EOG_VIEW_DITHER,
								    NULL));
	image_view_set_check_type (image_view->priv->image_view,
				   (CheckType) gconf_client_get_int (image_view->priv->client,
								     GCONF_EOG_VIEW_CHECK_TYPE,
								     NULL));
	image_view_set_check_size (image_view->priv->image_view,
				   (CheckSize) gconf_client_get_int (image_view->priv->client,
								     GCONF_EOG_VIEW_CHECK_SIZE,
								     NULL));
	image_view->priv->interp_type_notify_id = 
		gconf_client_notify_add (image_view->priv->client,
					 GCONF_EOG_VIEW_INTERP_TYPE, 
					 interp_type_changed_cb,
					 image_view, NULL, NULL);
	image_view->priv->dither_notify_id = 
		gconf_client_notify_add (image_view->priv->client,
					 GCONF_EOG_VIEW_DITHER, 
					 dither_changed_cb,
					 image_view, NULL, NULL);
	image_view->priv->check_type_notify_id = 
		gconf_client_notify_add (image_view->priv->client,
					 GCONF_EOG_VIEW_CHECK_TYPE, 
					 check_type_changed_cb,
					 image_view, NULL, NULL);
	image_view->priv->check_size_notify_id = 
		gconf_client_notify_add (image_view->priv->client,
					 GCONF_EOG_VIEW_CHECK_SIZE, 
					 check_size_changed_cb,
					 image_view, NULL, NULL);
	
	/* Property Bag */
	image_view->priv->property_bag = bonobo_property_bag_new (eog_image_view_get_prop,
								  eog_image_view_set_prop,
								  image_view);

	bonobo_property_bag_add (image_view->priv->property_bag, "interpolation", PROP_INTERPOLATION,
				 TC_GNOME_EOG_Interpolation, NULL, _("Interpolation"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "dither", PROP_DITHER,
				 TC_GNOME_EOG_Dither, NULL, _("Dither"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "check_type", PROP_CHECK_TYPE,
				 TC_GNOME_EOG_CheckType, NULL, _("Check Type"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "check_size", PROP_CHECK_SIZE,
				 TC_GNOME_EOG_CheckSize, NULL, _("Check Size"), 
				 BONOBO_PROPERTY_READABLE | BONOBO_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "image/width", PROP_IMAGE_WIDTH,
				 BONOBO_ARG_INT, NULL, _("Image Width"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "image/height", PROP_IMAGE_HEIGHT,
				 BONOBO_ARG_INT, NULL, _("Image Height"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "window/title", PROP_WINDOW_TITLE,
				 BONOBO_ARG_STRING, NULL, _("Window Title"),
				 BONOBO_PROPERTY_READABLE);
	bonobo_property_bag_add (image_view->priv->property_bag, "window/status", PROP_WINDOW_STATUS,
				 BONOBO_ARG_STRING, NULL, _("Statusbar Text"),
				 BONOBO_PROPERTY_READABLE);

	/* UI Component */
	image_view->priv->uic = bonobo_ui_component_new ("EogImageView");

	/* Finally, set the image */
	image_set_image_cb (image, image_view);

	return image_view;
}

EogImageView *
eog_image_view_new (EogImage *image,
		    gboolean  zoom_fit)
{
	EogImageView *image_view;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	if (getenv ("DEBUG_EOG"))
		g_message ("Creating EogImageView...");

	image_view = g_object_new (EOG_IMAGE_VIEW_TYPE, NULL);

	return eog_image_view_construct (image_view, image, zoom_fit);
}
