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
	BonoboUIComponent  *uic;
        BonoboZoomable     *zoomable;

	float               zoom_level;

        GtkWidget          *ui_image;
        GtkWidget          *image_view;
        GdkPixbuf          *pixbuf;
};

/* Parent object class in GTK hierarchy */
static BonoboControlClass *eog_image_viewer_parent_class;

static void eog_image_viewer_activate (BonoboControl *control, gboolean state);

static void
eog_image_viewer_destroy (GtkObject *object)
{
	EogImageViewer *image_viewer;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (object));
	
	image_viewer = EOG_IMAGE_VIEWER (object);

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
	BonoboControlClass *control_class = (BonoboControlClass *) klass;

	eog_image_viewer_parent_class = gtk_type_class (bonobo_control_get_type ());

	control_class->activate = eog_image_viewer_activate;

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

	view = IMAGE_VIEW (image_viewer->priv->image_view);

	image_view_set_zoom (view, (double) new_zoom_level);
	image_viewer->priv->zoom_level = image_view_get_zoom (view);

	bonobo_zoomable_report_zoom_level_changed (zoomable,
						   image_viewer->priv->zoom_level);
}

static float preferred_zoom_levels[] = {
	1.0 / 10.0, 1.0 / 9.0, 1.0 / 8.0, 1.0 / 7.0, 1.0 / 6.0,
	1.0 / 5.0, 1.0 / 4.0, 1.0 / 3.0, 1.0 / 2.0, 1.0, 2.0,
	3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0
};
static const gchar *preferred_zoom_level_names[] = {
	"1:10", "1:9", "1:8", "1:7", "1:6", "1:5", "1:4", "1:3",
	"1:2", "1:1", "2:1", "3:1", "4:1", "5:1", "6:1", "7:1",
	"8:1", "9:1", "10:1"
};

static const gint max_preferred_zoom_levels = (sizeof (preferred_zoom_levels) /
					       sizeof (float)) - 1;

static int
zoom_index_from_float (float zoom_level)
{
	int i;

	for (i = 0; i < max_preferred_zoom_levels; i++) {
		float this, epsilon;

		/* if we're close to a zoom level */
		this = preferred_zoom_levels [i];
		epsilon = this * 0.01;

		if (zoom_level < this+epsilon)
			return i;
	}

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

static void
listener_Interpolation_cb (BonoboUIComponent *uic, const char *path,
			   Bonobo_UIComponent_EventType type, const char *state,
			   gpointer user_data)
{
	EogImageViewer *image_viewer;
	GdkInterpType interp_type;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_viewer = EOG_IMAGE_VIEWER (user_data);

	if (!strcmp (path, "InterpolationNearest"))
		interp_type = GDK_INTERP_NEAREST;
	else if (!strcmp (path, "InterpolationTiles"))
		interp_type = GDK_INTERP_TILES;
	else if (!strcmp (path, "InterpolationBilinear"))
		interp_type = GDK_INTERP_BILINEAR;
	else if (!strcmp (path, "InterpolationHyperbolic"))
		interp_type = GDK_INTERP_HYPER;
	else {
		g_warning ("Unknown interpolation type `%s'", path);
		return;
	}

	image_view_set_interp_type (IMAGE_VIEW (image_viewer->priv->image_view), interp_type);
}

static void
listener_Dither_cb (BonoboUIComponent *uic, const char *path,
		    Bonobo_UIComponent_EventType type, const char *state,
		    gpointer user_data)
{
	EogImageViewer *image_viewer;
	GdkRgbDither dither;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_viewer = EOG_IMAGE_VIEWER (user_data);

	if (!strcmp (path, "DitherNone"))
		dither = GDK_RGB_DITHER_NONE;
	else if (!strcmp (path, "DitherNormal"))
		dither = GDK_RGB_DITHER_NORMAL;
	else if (!strcmp (path, "DitherMaximum"))
		dither = GDK_RGB_DITHER_MAX;
	else {
		g_warning ("Unknown dither type `%s'", path);
		return;
	}

	image_view_set_dither (IMAGE_VIEW (image_viewer->priv->image_view), dither);
}

static void
listener_CheckType_cb (BonoboUIComponent *uic, const char *path,
		       Bonobo_UIComponent_EventType type, const char *state,
		       gpointer user_data)
{
	EogImageViewer *image_viewer;
	CheckType check_type;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_viewer = EOG_IMAGE_VIEWER (user_data);

	if (!strcmp (path, "CheckTypeDark"))
		check_type = CHECK_TYPE_DARK;
	else if (!strcmp (path, "CheckTypeMidtone"))
		check_type = CHECK_TYPE_MIDTONE;
	else if (!strcmp (path, "CheckTypeLight"))
		check_type = CHECK_TYPE_LIGHT;
	else if (!strcmp (path, "CheckTypeBlack"))
		check_type = CHECK_TYPE_BLACK;
	else if (!strcmp (path, "CheckTypeGray"))
		check_type = CHECK_TYPE_GRAY;
	else if (!strcmp (path, "CheckTypeWhite"))
		check_type = CHECK_TYPE_WHITE;
	else {
		g_warning ("Unknown check type `%s'", path);
		return;
	}

	image_view_set_check_type (IMAGE_VIEW (image_viewer->priv->image_view), check_type);
}

static void
listener_CheckSize_cb (BonoboUIComponent *uic, const char *path,
		       Bonobo_UIComponent_EventType type, const char *state,
		       gpointer user_data)
{
	EogImageViewer *image_viewer;
	CheckSize check_size;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (user_data));

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!state || !atoi (state))
		return;

	image_viewer = EOG_IMAGE_VIEWER (user_data);

	if (!strcmp (path, "CheckSizeSmall"))
		check_size = CHECK_SIZE_SMALL;
	else if (!strcmp (path, "CheckSizeMedium"))
		check_size = CHECK_SIZE_MEDIUM;
	else if (!strcmp (path, "CheckSizeLarge"))
		check_size = CHECK_SIZE_LARGE;
	else {
		g_warning ("Unknown check size `%s'", path);
		return;
	}

	image_view_set_check_size (IMAGE_VIEW (image_viewer->priv->image_view), check_size);
}

static BonoboUIVerb eog_image_viewer_verbs[] = {
	BONOBO_UI_VERB_END
};

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

/*
 * When we are activated, merge our menus with our container's menus.
 */
static void
eog_image_viewer_create_menus (EogImageViewer *image_viewer)
{
	Bonobo_UIContainer remote_uic;

	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));

	/*
	 * Get our UIComponent from the Control.
	 */
	image_viewer->priv->uic = bonobo_control_get_ui_component (BONOBO_CONTROL (image_viewer));

	/*
	 * Get our container's UIContainer server.
	 */
	remote_uic = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (image_viewer));

	/*
	 * We have to deal gracefully with containers
	 * which don't have a UIContainer running.
	 */
	if (remote_uic == CORBA_OBJECT_NIL) {
		g_warning ("No UI container!");
		return;
	}

	/*
	 * Give our BonoboUIComponent object a reference to the
	 * container's UIContainer server.
	 */
	bonobo_ui_component_set_container (image_viewer->priv->uic, remote_uic);

	/*
	 * Unref the UI container we have been passed.
	 */
	bonobo_object_release_unref (remote_uic, NULL);

	/* Set up the UI from an XML file. */
        bonobo_ui_util_set_ui (image_viewer->priv->uic, DATADIR,
			       "eog-image-viewer-ui.xml", "EogImageViewer");

	bonobo_ui_component_add_listener (image_viewer->priv->uic, "InterpolationNearest",
					  listener_Interpolation_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "InterpolationTiles",
					  listener_Interpolation_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "InterpolationBilinear",
					  listener_Interpolation_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "InterpolationHyperbolic",
					  listener_Interpolation_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "DitherNone",
					  listener_Dither_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "DitherNormal",
					  listener_Dither_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "DitherMaximum",
					  listener_Dither_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckTypeDark",
					  listener_CheckType_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckTypeMidtone",
					  listener_CheckType_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckTypeLight",
					  listener_CheckType_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckTypeBlack",
					  listener_CheckType_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckTypeGray",
					  listener_CheckType_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckTypeWhite",
					  listener_CheckType_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckSizeSmall",
					  listener_CheckSize_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckSizeMedium",
					  listener_CheckSize_cb, image_viewer);
	bonobo_ui_component_add_listener (image_viewer->priv->uic, "CheckSizeLarge",
					  listener_CheckSize_cb, image_viewer);

	bonobo_ui_component_add_verb_list_with_data (image_viewer->priv->uic,
						     eog_image_viewer_verbs,
						     image_viewer);
}

/*
 * When we are deactivated, unmerge our menus again.
 */
static void
eog_image_viewer_remove_menus (EogImageViewer *image_viewer)
{
	g_return_if_fail (image_viewer != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer));

	if (image_viewer->priv->uic)
		bonobo_ui_component_unset_container (image_viewer->priv->uic);
}

static void
eog_image_viewer_activate (BonoboControl *control, gboolean state)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_IMAGE_VIEWER (control));

	if (BONOBO_CONTROL_CLASS (eog_image_viewer_parent_class)->activate)
		BONOBO_CONTROL_CLASS (eog_image_viewer_parent_class)->activate (control, state);

	if (state)
		eog_image_viewer_create_menus (EOG_IMAGE_VIEWER (control));
	else
		eog_image_viewer_remove_menus (EOG_IMAGE_VIEWER (control));
}


EogImageViewer *
eog_image_viewer_construct (EogImageViewer *image_viewer, Bonobo_Control corba_control)
{
	BonoboControl        *retval;
	BonoboPersistStream  *stream;
	BonoboPersistFile    *file;

	g_return_val_if_fail (image_viewer != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEWER (image_viewer), NULL);

	image_viewer->priv->ui_image = ui_image_new ();
	image_viewer->priv->image_view = ui_image_get_image_view (UI_IMAGE (image_viewer->priv->ui_image));
	gtk_widget_ref (image_viewer->priv->image_view);

	gtk_widget_show (image_viewer->priv->ui_image);

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

	image_viewer->priv->zoom_level = 1.0;
	bonobo_zoomable_set_parameters_full (image_viewer->priv->zoomable,
					     image_viewer->priv->zoom_level,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [max_preferred_zoom_levels],
					     FALSE, FALSE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     max_preferred_zoom_levels + 1);

	bonobo_object_add_interface (BONOBO_OBJECT (image_viewer),
				     BONOBO_OBJECT (image_viewer->priv->zoomable));

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

