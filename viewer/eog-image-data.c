/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-data.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000 SuSE GmbH.
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include <eog-image-data.h>
#include <image.h>

struct _EogImageDataPrivate {
        Image      *image;
        GdkPixbuf  *pixbuf;
};

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static BonoboObjectClass *eog_image_data_parent_class;

enum {
	SET_IMAGE_SIGNAL,
	LAST_SIGNAL
};

static guint eog_image_data_signals [LAST_SIGNAL];

static void
eog_image_data_destroy (GtkObject *object)
{
	EogImageData *image_data;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_DATA (object));

	image_data = EOG_IMAGE_DATA (object);

	if (image_data->priv->image) {
		image_unref (image_data->priv->image);
		image_data->priv->image = NULL;
	}

	if (image_data->priv->pixbuf) {
		gdk_pixbuf_unref (image_data->priv->pixbuf);
		image_data->priv->pixbuf = NULL;
	}

	GTK_OBJECT_CLASS (eog_image_data_parent_class)->destroy (object);
}

static void
eog_image_data_finalize (GtkObject *object)
{
	EogImageData *image_data;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_DATA (object));

	image_data = EOG_IMAGE_DATA (object);

	g_free (image_data->priv);

	GTK_OBJECT_CLASS (eog_image_data_parent_class)->finalize (object);
}

static void
eog_image_data_class_init (EogImageDataClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	POA_GNOME_EOG_ImageData__epv *epv;

	eog_image_data_parent_class = gtk_type_class (PARENT_TYPE);

	eog_image_data_signals [SET_IMAGE_SIGNAL] =
                gtk_signal_new ("set_image",
                                GTK_RUN_LAST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (EogImageDataClass, set_image),
                                gtk_marshal_NONE__NONE,
                                GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, eog_image_data_signals, LAST_SIGNAL);

	object_class->destroy = eog_image_data_destroy;
	object_class->finalize = eog_image_data_finalize;

	epv = &klass->epv;
}

static void
eog_image_data_init (EogImageData *image_data)
{
	image_data->priv = g_new0 (EogImageDataPrivate, 1);
}

BONOBO_X_TYPE_FUNC_FULL (EogImageData,
			 GNOME_EOG_ImageData,
			 PARENT_TYPE,
			 eog_image_data);

/*
 * Loads an Image from a Bonobo_Stream
 */
static void
load_image_from_stream (BonoboPersistStream *ps, Bonobo_Stream stream,
			Bonobo_Persist_ContentType type, void *data,
			CORBA_Environment *ev)
{
	EogImageData         *image_data;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	Bonobo_Stream_iobuf  *buffer;
	CORBA_long            len;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE_DATA (data));

	image_data = EOG_IMAGE_DATA (data);

	if (image_data->priv->pixbuf)
		gdk_pixbuf_unref (image_data->priv->pixbuf);
	image_data->priv->pixbuf = NULL;

	if (image_data->priv->image)
		image_unref (image_data->priv->image);
	image_data->priv->image = NULL;

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

	image_data->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!image_data->priv->pixbuf)
		goto exit_wrong_type;

	gdk_pixbuf_ref (image_data->priv->pixbuf);

	image_data->priv->image = image_new ();
	image_load_pixbuf (image_data->priv->image, image_data->priv->pixbuf);

	gtk_signal_emit (GTK_OBJECT (image_data), eog_image_data_signals [SET_IMAGE_SIGNAL]);

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
load_image_from_file (BonoboPersistFile *pf, const CORBA_char *filename,CORBA_Environment *ev,
		      void *closure)
{
	EogImageData *image_data;

	g_return_val_if_fail (closure != NULL, -1);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (closure), -1);

	image_data = EOG_IMAGE_DATA (closure);

	if (image_data->priv->pixbuf)
		gdk_pixbuf_unref (image_data->priv->pixbuf);
	image_data->priv->pixbuf = NULL;

	if (image_data->priv->image)
		image_unref (image_data->priv->image);
	image_data->priv->image = NULL;

	image_data->priv->pixbuf = gdk_pixbuf_new_from_file (filename);
	if (!image_data->priv->pixbuf)
		return -1;

	gdk_pixbuf_ref (image_data->priv->pixbuf);

	image_data->priv->image = image_new ();
	image_load_pixbuf (image_data->priv->image, image_data->priv->pixbuf);

	gtk_signal_emit (GTK_OBJECT (image_data), eog_image_data_signals [SET_IMAGE_SIGNAL]);

	return 0;
}

EogImageData *
eog_image_data_construct (EogImageData *image_data)
{
	BonoboPersistStream *stream;
	BonoboPersistFile *file;
	BonoboObject *retval;

	g_return_val_if_fail (image_data != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (image_data), NULL);
	
	/*
	 * Interface Bonobo::PersistStream 
	 */
	stream = bonobo_persist_stream_new (load_image_from_stream, 
					    NULL, NULL, NULL, image_data);
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (image_data));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (image_data),
				     BONOBO_OBJECT (stream));

	/*
	 * Interface Bonobo::PersistFile 
	 */
	file = bonobo_persist_file_new (load_image_from_file, NULL, image_data);
	if (file == NULL) {
		gtk_object_unref (GTK_OBJECT (image_data));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (image_data),
				     BONOBO_OBJECT (file));

	return image_data;
}

EogImageData *
eog_image_data_new (void)
{
	EogImageData *image_data;
	
	image_data = gtk_type_new (eog_image_data_get_type ());

	return eog_image_data_construct (image_data);
}

Image *
eog_image_data_get_image (EogImageData *image_data)
{
	g_return_val_if_fail (image_data != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (image_data), NULL);

	image_ref (image_data->priv->image);
	return image_data->priv->image;
}

