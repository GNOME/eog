/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * EOG image viewer control the core eog code.
 *
 * Authors:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, Eazel, Inc.
 * Copyright 2000, SuSE GmbH.
 */

#include <ui-image.h>
#include <image-view.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include <liboaf/liboaf.h>
#include <liboaf/oaf-mainloop.h>

#include "eog-image-viewer.h"

struct _EogImageViewerPrivate
{
        BonoboControl   *control;
        BonoboZoomable  *zoomable;

	float            zoom_level;

        GtkWidget       *ui_image;
        GtkWidget       *image_view;
        GdkPixbuf       *pixbuf;
};

/* Parent object class in GTK hierarchy */
static BonoboControlClass *eog_image_viewer_parent_class;

static void
eog_image_viewer_destroy (GtkObject *object)
{
	EogImageViewer *image_viewer;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (object));
	
	image_viewer = EOG_IMAGE_VIEWER (object);

	g_message ("eog_image_viewer_destroy: %p", image_viewer);

	if (image_viewer->priv) {
		if (image_viewer->priv->image_view)
			gtk_widget_unref (image_viewer->priv->image_view);
		image_viewer->priv->image_view = NULL;

		if (image_viewer->priv->pixbuf)
			gdk_pixbuf_unref (image_viewer->priv->pixbuf);
		image_viewer->priv->pixbuf = NULL;
	}

	g_free (image_viewer->priv);
	image_viewer->priv = NULL;
	
	GTK_OBJECT_CLASS (eog_image_viewer_parent_class)->destroy (object);
}

static void
eog_image_viewer_class_init (EogImageViewerClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	eog_image_viewer_parent_class = gtk_type_class (bonobo_control_get_type ());

	object_class->destroy = eog_image_viewer_destroy;
}

static void
eog_image_viewer_init (EogImageViewer *image_viewer)
{
	image_viewer->priv = g_new0 (EogImageViewerPrivate, 1);
}

static void
zoomable_set_zoom_level_cb (BonoboZoomable *zoomable, float new_zoom_level,
			    EogImageViewer *image_viewer)
{
	ImageView *view;

	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));

	g_message ("New zoom level: %5.2f", new_zoom_level);

	view = IMAGE_VIEW (image_viewer->priv->image_view);

	image_view_set_zoom (view, (double) new_zoom_level);
	image_viewer->priv->zoom_level = image_view_get_zoom (view);

	bonobo_zoomable_report_zoom_level_changed (zoomable,
						   image_viewer->priv->zoom_level);


	g_message ("Done zooming to %5.2f", image_viewer->priv->zoom_level);
}

static float preferred_zoom_levels[] = {
	1.0 / 10.0, 1.0 /  9.0, 1.0 /  8.0, 1.0 /  7.0, 1.0 /  6.0,
	1.0 /  5.0, 1.0 /  4.0, 1.0 /  3.0, 1.0 /  2.0,
	1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0
};
static const int max_preferred_zoom_levels = (sizeof (preferred_zoom_levels) / sizeof (float)) - 1;

static int
zoom_index_from_float (float zoom_level)
{
	int i;

	for (i = 0; i < max_preferred_zoom_levels; i++)
		if (zoom_level < preferred_zoom_levels [i])
			return i;

	return max_preferred_zoom_levels;
}

static float
zoom_level_from_index (int index)
{
	if (index > max_preferred_zoom_levels)
		index = max_preferred_zoom_levels;

	return preferred_zoom_levels [index];
}

static void
zoomable_zoom_in_cb (BonoboZoomable *zoomable, EogImageViewer *image_viewer)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));

	index = zoom_index_from_float (image_viewer->priv->zoom_level);
	if (index == max_preferred_zoom_levels)
		return;

	index++;
	new_zoom_level = zoom_level_from_index (index);

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", new_zoom_level);
}

static void
zoomable_zoom_out_cb (BonoboZoomable *zoomable, EogImageViewer *image_viewer)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));

	index = zoom_index_from_float (image_viewer->priv->zoom_level);
	if (index == 0)
		return;

	index--;
	new_zoom_level = zoom_level_from_index (index);

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", new_zoom_level);
}

static void
zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable, EogImageViewer *image_viewer)
{
	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));
}

static void
zoomable_zoom_to_default_cb (BonoboZoomable *zoomable, EogImageViewer *image_viewer)
{
	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level", 1.0);
}


/**
 * eog_image_viewer_get_type:
 *
 * Returns: The GtkType corresponding to the EogImageViewer class.
 */
GtkType
eog_image_viewer_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogImageViewer",
			sizeof (EogImageViewer),
			sizeof (EogImageViewerClass),
			(GtkClassInitFunc) eog_image_viewer_class_init,
			(GtkObjectInitFunc) eog_image_viewer_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_control_get_type (), &info);
	}

	return type;
}

/*
 * Loads an Image from a Bonobo_Stream
 */
static void
load_image_from_stream (BonoboPersistStream *ps, Bonobo_Stream stream,
			Bonobo_Persist_ContentType type, void *data,
			CORBA_Environment *ev)
{
	EogImageViewer       *image_viewer;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	Image                *image;
	Bonobo_Stream_iobuf  *buffer;
	CORBA_long            len;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (data));

	image_viewer = EOG_IMAGE_VIEWER (data);

	if (image_viewer->priv->pixbuf)
		gdk_pixbuf_unref (image_viewer->priv->pixbuf);
	image_viewer->priv->pixbuf = NULL;

	do {
		Bonobo_Stream_read (stream, 4096, &buffer, ev);
		if (ev->_major != CORBA_NO_EXCEPTION)
			goto exit_clean;

		if (buffer->_buffer &&
		     !gdk_pixbuf_loader_write (loader,
					       buffer->_buffer,
					       buffer->_length)) {
			CORBA_free (buffer);
			if (ev->_major == CORBA_NO_EXCEPTION)
				goto exit_clean;
			else
				goto exit_wrong_type;
		}
		len = buffer->_length;

		CORBA_free (buffer);
	} while (len > 0);

	image_viewer->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!image_viewer->priv->pixbuf)
		goto exit_wrong_type;

	gdk_pixbuf_ref (image_viewer->priv->pixbuf);

	image = image_new ();
	image_load_pixbuf (image, image_viewer->priv->pixbuf);

	image_view_set_image (IMAGE_VIEW (image_viewer->priv->image_view), image);

	goto exit_clean;

 exit_wrong_type:
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

 exit_clean:
	gdk_pixbuf_loader_close (loader);
	gtk_object_unref (GTK_OBJECT (loader));
	return;
}

/*
 * Loads an Image from a Bonobo_File
 */
static gint
load_image_from_file (BonoboPersistFile *pf, const CORBA_char *filename, void *data)
{
	EogImageViewer *image_viewer;
	Image *image;

	g_return_val_if_fail (data != NULL, -1);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEWER (data), -1);

	image_viewer = EOG_IMAGE_VIEWER (data);

	g_message ("load_image_from_file: `%s'", filename);

	if (image_viewer->priv->pixbuf)
		gdk_pixbuf_unref (image_viewer->priv->pixbuf);

	image_viewer->priv->pixbuf = gdk_pixbuf_new_from_file (filename);

	if (!image_viewer->priv->pixbuf)
		return -1;

	gdk_pixbuf_ref (image_viewer->priv->pixbuf);

	image = image_new ();
	image_load_pixbuf (image, image_viewer->priv->pixbuf);

	image_view_set_image (IMAGE_VIEW (image_viewer->priv->image_view), image);

	bonobo_zoomable_report_zoom_level_changed (image_viewer->priv->zoomable, 1.0);

	return 0;
}

EogImageViewer *
eog_image_viewer_construct (EogImageViewer *image_viewer, Bonobo_Control corba_control)
{
	CORBA_Environment     ev;
	BonoboControl        *retval;
	BonoboPersistStream  *stream;
	BonoboPersistFile    *file;
	gchar                *ior;

	g_return_val_if_fail (image_viewer != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer), NULL);

	image_viewer->priv->ui_image = ui_image_new ();
	image_viewer->priv->image_view = ui_image_get_image_view (UI_IMAGE (image_viewer->priv->ui_image));
	gtk_widget_ref (image_viewer->priv->image_view);

	gtk_widget_show (image_viewer->priv->ui_image);

	CORBA_exception_init (&ev);
	ior = CORBA_ORB_object_to_string (oaf_orb_get (), corba_control, &ev);
	g_message ("Control IOR is `%s'", ior);
	CORBA_exception_free (&ev);

	/*
	 * Interface Bonobo::Zoomable 
	 */
	image_viewer->priv->zoomable = bonobo_zoomable_new ();
	if (image_viewer->priv->zoomable == NULL) {
		gtk_object_unref (GTK_OBJECT (image_viewer));
		return NULL;
	}

	gtk_signal_connect (GTK_OBJECT (image_viewer->priv->zoomable), "set_zoom_level",
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_cb), image_viewer);
	gtk_signal_connect (GTK_OBJECT (image_viewer->priv->zoomable), "zoom_in",
			    GTK_SIGNAL_FUNC (zoomable_zoom_in_cb), image_viewer);
	gtk_signal_connect (GTK_OBJECT (image_viewer->priv->zoomable), "zoom_out",
			    GTK_SIGNAL_FUNC (zoomable_zoom_out_cb), image_viewer);
	gtk_signal_connect (GTK_OBJECT (image_viewer->priv->zoomable), "zoom_to_fit",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_fit_cb), image_viewer);
	gtk_signal_connect (GTK_OBJECT (image_viewer->priv->zoomable), "zoom_to_default",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_default_cb), image_viewer);

	CORBA_exception_init (&ev);
	ior = CORBA_ORB_object_to_string (oaf_orb_get (),
		bonobo_object_corba_objref (BONOBO_OBJECT (image_viewer->priv->zoomable)), &ev);
	g_message ("Zoomable IOR is `%s'", ior);
	CORBA_exception_free (&ev);

	image_viewer->priv->zoom_level = 1.0;
	bonobo_zoomable_set_parameters (image_viewer->priv->zoomable,
					image_viewer->priv->zoom_level,
					preferred_zoom_levels [0],
					preferred_zoom_levels [max_preferred_zoom_levels],
					FALSE, FALSE, TRUE,
					preferred_zoom_levels, max_preferred_zoom_levels + 1);

	bonobo_object_add_interface (BONOBO_OBJECT (image_viewer),
				     BONOBO_OBJECT (image_viewer->priv->zoomable));

	g_message ("Added zoomable interface");

	/*
	 * Interface Bonobo::PersistStream 
	 */
	stream = bonobo_persist_stream_new (load_image_from_stream, 
					    NULL, NULL, NULL, image_viewer);
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (image_viewer));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (image_viewer),
				     BONOBO_OBJECT (stream));

	/*
	 * Interface Bonobo::PersistFile 
	 */
	file = bonobo_persist_file_new (load_image_from_file, NULL, image_viewer);
	if (file == NULL) {
		gtk_object_unref (GTK_OBJECT (image_viewer));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (image_viewer),
				     BONOBO_OBJECT (file));

	/*
	 * Construct the BonoboControl.
	 */
	retval = bonobo_control_construct (BONOBO_CONTROL (image_viewer),
					   corba_control,
					   image_viewer->priv->ui_image);

	g_message ("Successfully constructed the EogImageViewer %p", retval);

	return EOG_IMAGE_VIEWER (retval);
}

/**
 * eog_image_viewer_new:
 *
 * This function creates a new EogImageViewer.
 *
 * Returns: the newly created EogImageViewer.
 */
EogImageViewer *
eog_image_viewer_new (void)
{
	Bonobo_Control corba_control;
	EogImageViewer *image_viewer;
	
	image_viewer = gtk_type_new (eog_image_viewer_get_type ());

	corba_control = bonobo_control_corba_object_create (BONOBO_OBJECT (image_viewer));
	if (corba_control == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (image_viewer));
		return NULL;
	}

	eog_image_viewer_construct (image_viewer, corba_control);

	return image_viewer;
}

