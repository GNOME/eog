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

#include <eog-image.h>
#include <eog-control.h>
#include <eog-embeddable.h>

struct _EogImagePrivate {
        Image                *image;
        GdkPixbuf            *pixbuf;

	BonoboItemContainer  *item_container;
};

POA_GNOME_EOG_Image__vepv eog_image_vepv;

static BonoboObjectClass *eog_image_parent_class;

enum {
	SET_IMAGE_SIGNAL,
	LAST_SIGNAL
};

static guint eog_image_signals [LAST_SIGNAL];

/**
 * eog_image_get_epv:
 */
POA_GNOME_EOG_Image__epv *
eog_image_get_epv (void)
{
	POA_GNOME_EOG_Image__epv *epv;

	epv = g_new0 (POA_GNOME_EOG_Image__epv, 1);

	return epv;
}

static void
init_eog_image_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_image_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_image_vepv.GNOME_EOG_Image_epv = eog_image_get_epv ();
}

static void
eog_image_destroy (GtkObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

	if (image->priv->image) {
		image_unref (image->priv->image);
		image->priv->image = NULL;
	}

	if (image->priv->pixbuf) {
		gdk_pixbuf_unref (image->priv->pixbuf);
		image->priv->pixbuf = NULL;
	}

	if (image->priv->item_container) {
		bonobo_object_unref (BONOBO_OBJECT (image->priv->item_container));
		image->priv->item_container = NULL;
	}

	GTK_OBJECT_CLASS (eog_image_parent_class)->destroy (object);
}

static void
eog_image_finalize (GtkObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

	g_free (image->priv);

	GTK_OBJECT_CLASS (eog_image_parent_class)->finalize (object);
}

static void
eog_image_class_init (EogImageClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_image_parent_class = gtk_type_class (bonobo_object_get_type ());

	gtk_object_class_add_signals (object_class, eog_image_signals, LAST_SIGNAL);

	eog_image_signals [SET_IMAGE_SIGNAL] =
                gtk_signal_new ("set_image",
                                GTK_RUN_LAST,
                                object_class->type,
                                GTK_SIGNAL_OFFSET (EogImageClass, set_image),
                                gtk_marshal_NONE__NONE,
                                GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, eog_image_signals,
				      LAST_SIGNAL);

	object_class->destroy = eog_image_destroy;
	object_class->finalize = eog_image_finalize;

	init_eog_image_corba_class ();
}

static void
eog_image_init (EogImage *image)
{
	image->priv = g_new0 (EogImagePrivate, 1);
}

GtkType
eog_image_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogImage",
			sizeof (EogImage),
			sizeof (EogImageClass),
			(GtkClassInitFunc) eog_image_class_init,
			(GtkObjectInitFunc) eog_image_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_object_get_type (), &info);
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
	EogImage             *image;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	Bonobo_Stream_iobuf  *buffer;
	CORBA_long            len;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE (data));

	image = EOG_IMAGE (data);

	if (image->priv->pixbuf)
		gdk_pixbuf_unref (image->priv->pixbuf);
	image->priv->pixbuf = NULL;

	if (image->priv->image)
		image_unref (image->priv->image);
	image->priv->image = NULL;

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

	image->priv->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!image->priv->pixbuf)
		goto exit_wrong_type;

	gdk_pixbuf_ref (image->priv->pixbuf);

	image->priv->image = image_new ();
	image_load_pixbuf (image->priv->image, image->priv->pixbuf);

	gtk_signal_emit (GTK_OBJECT (image), eog_image_signals [SET_IMAGE_SIGNAL]);

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
 * Saves an Image to a Bonobo_Stream
 */
static void
save_image_to_stream (BonoboPersistStream *ps, Bonobo_Stream stream,
		      Bonobo_Persist_ContentType type, void *data,
		      CORBA_Environment *ev)
{
	EogImage *image;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_IMAGE (data));

	image = EOG_IMAGE (data);

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

	return;
}



/*
 * Loads an Image from a Bonobo_File
 */
static gint
load_image_from_file (BonoboPersistFile *pf, const CORBA_char *filename,
		      CORBA_Environment *ev, void *closure)
{
	EogImage *image;

	g_return_val_if_fail (closure != NULL, -1);
	g_return_val_if_fail (EOG_IS_IMAGE (closure), -1);

	image = EOG_IMAGE (closure);

	if (image->priv->pixbuf)
		gdk_pixbuf_unref (image->priv->pixbuf);
	image->priv->pixbuf = NULL;

	if (image->priv->image)
		image_unref (image->priv->image);
	image->priv->image = NULL;

	image->priv->pixbuf = gdk_pixbuf_new_from_file (filename);
	if (!image->priv->pixbuf)
		return -1;

	gdk_pixbuf_ref (image->priv->pixbuf);

	image->priv->image = image_new ();
	image_load_pixbuf (image->priv->image, image->priv->pixbuf);

	gtk_signal_emit (GTK_OBJECT (image), eog_image_signals [SET_IMAGE_SIGNAL]);

	return 0;
}

GNOME_EOG_Image
eog_image_corba_object_create (BonoboObject *object)
{
	POA_GNOME_EOG_Image *servant;
	CORBA_Environment ev;
	
	servant = (POA_GNOME_EOG_Image *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_image_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_EOG_Image__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (GNOME_EOG_Image) bonobo_object_activate_servant (object, servant);
}

static Bonobo_Unknown
eog_image_get_object (BonoboItemContainer *item_container,
		      CORBA_char *item_name, CORBA_boolean only_if_exists,
		      CORBA_Environment *ev, EogImage *image)
{
	Bonobo_Unknown corba_object;
	BonoboObject *object = NULL;
	GSList *params, *c;

	g_return_val_if_fail (image != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), CORBA_OBJECT_NIL);

	g_message ("eog_image_get_object: %d - %s",
		   only_if_exists, item_name);

	params = eog_util_split_string (item_name, ";");
	for (c = params; c; c = c->next) {
		gchar *name = c->data;

		if ((!strcmp (name, "control") || !strcmp (name, "embeddable"))
		    && (object != NULL)) {
			g_message ("eog_image_get_object: "
				   "can only return one kind of an Object");
			continue;
		}

		if (!strcmp (name, "control"))
			object = (BonoboObject *) eog_control_new (image);
		else if (!strcmp (item_name, "embeddable"))
			object = (BonoboObject *) eog_embeddable_new (image);
		else
			g_message ("eog_image_get_object: "
				   "unknown parameter `%s'",
				   name);
	}

	g_slist_foreach (params, (GFunc) g_free, NULL);
	g_slist_free (params);

	if (object == NULL)
		return NULL;

	corba_object = bonobo_object_corba_objref (object);
	return bonobo_object_dup_ref (corba_object, ev);
}

EogImage *
eog_image_construct (EogImage *image, GNOME_EOG_Image corba_object)
{
	BonoboPersistStream *stream;
	BonoboPersistFile *file;
	BonoboObject *retval;

	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);

	/*
	 * Interface Bonobo::PersistStream 
	 */
	stream = bonobo_persist_stream_new (load_image_from_stream, 
					    save_image_to_stream,
					    NULL, NULL, image);
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (image));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (image),
				     BONOBO_OBJECT (stream));

	/*
	 * Interface Bonobo::PersistFile 
	 */
	file = bonobo_persist_file_new (load_image_from_file, NULL, image);
	if (file == NULL) {
		gtk_object_unref (GTK_OBJECT (image));
		return NULL;
	}

	bonobo_object_add_interface (BONOBO_OBJECT (image),
				     BONOBO_OBJECT (file));

	/*
	 * BonoboItemContainer
	 */
	image->priv->item_container = bonobo_item_container_new ();

	gtk_signal_connect (GTK_OBJECT (image->priv->item_container),
			    "get_object",
			    GTK_SIGNAL_FUNC (eog_image_get_object),
			    image);

	bonobo_object_add_interface (BONOBO_OBJECT (image),
				     BONOBO_OBJECT (image->priv->item_container));

	/*
	 * Construct the BonoboObject
	 */
	retval = bonobo_object_construct (BONOBO_OBJECT (image), corba_object);
	if (retval == NULL)
		return NULL;

	return image;
}

EogImage *
eog_image_new (void)
{
	EogImage *image;
	GNOME_EOG_Image corba_object;
	
	image = gtk_type_new (eog_image_get_type ());

	corba_object = eog_image_corba_object_create (BONOBO_OBJECT (image));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (image));
		return NULL;
	}
	
	return eog_image_construct (image, corba_object);
}

Image *
eog_image_get_image (EogImage *image)
{
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	image_ref (image->priv->image);
	return image->priv->image;
}

GdkPixbuf *
eog_image_get_pixbuf (EogImage *image)
{
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	gdk_pixbuf_ref (image->priv->pixbuf);
	return image->priv->pixbuf;
}
