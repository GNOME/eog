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

struct _EogImagePrivate {
};

POA_GNOME_EOG_Image__vepv eog_image_vepv;

static EogImageDataClass *eog_image_parent_class;

enum {
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
	eog_image_vepv.GNOME_EOG_ImageData_epv = eog_image_data_get_epv ();
	eog_image_vepv.GNOME_EOG_Image_epv = eog_image_get_epv ();
}

static void
eog_image_destroy (GtkObject *object)
{
	EogImage *image;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE (object));

	image = EOG_IMAGE (object);

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

	eog_image_parent_class = gtk_type_class (eog_image_data_get_type ());

	gtk_object_class_add_signals (object_class, eog_image_signals, LAST_SIGNAL);

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

		type = gtk_type_unique (eog_image_data_get_type (), &info);
	}

	return type;
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

EogImage *
eog_image_construct (EogImage *image, GNOME_EOG_Image corba_object)
{
	EogImageData *retval;

	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	
	/*
	 * Construct the BonoboObject
	 */
	retval = eog_image_data_construct (EOG_IMAGE_DATA (image), corba_object);
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
