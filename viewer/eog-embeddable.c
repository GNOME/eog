/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-embeddable.c
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

#include <eog-embeddable.h>
#include <eog-embeddable-view.h>

struct _EogEmbeddablePrivate {
	EogImageData       *image_data;
};

POA_Bonobo_Embeddable__vepv eog_embeddable_vepv;

static BonoboEmbeddableClass *eog_embeddable_parent_class;

static void
init_eog_embeddable_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_embeddable_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_embeddable_vepv.Bonobo_Embeddable_epv = bonobo_embeddable_get_epv ();
}

static void
eog_embeddable_destroy (GtkObject *object)
{
	EogEmbeddable *embeddable;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE (object));

	embeddable = EOG_EMBEDDABLE (object);

	if (embeddable->priv->image_data) {
		bonobo_object_unref (BONOBO_OBJECT (embeddable->priv->image_data));
		embeddable->priv->image_data = NULL;
	}

	GTK_OBJECT_CLASS (eog_embeddable_parent_class)->destroy (object);
}

static void
eog_embeddable_finalize (GtkObject *object)
{
	EogEmbeddable *embeddable;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_EMBEDDABLE (object));

	embeddable = EOG_EMBEDDABLE (object);

	g_free (embeddable->priv);

	GTK_OBJECT_CLASS (eog_embeddable_parent_class)->finalize (object);
}

static void
eog_embeddable_class_init (EogEmbeddable *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_embeddable_parent_class = gtk_type_class (bonobo_embeddable_get_type ());

	object_class->destroy = eog_embeddable_destroy;
	object_class->finalize = eog_embeddable_finalize;

	init_eog_embeddable_corba_class ();
}

static void
eog_embeddable_init (EogEmbeddable *embeddable)
{
	embeddable->priv = g_new0 (EogEmbeddablePrivate, 1);
}

GtkType
eog_embeddable_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogEmbeddable",
			sizeof (EogEmbeddable),
			sizeof (EogEmbeddableClass),
			(GtkClassInitFunc) eog_embeddable_class_init,
			(GtkObjectInitFunc) eog_embeddable_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_embeddable_get_type (), &info);
	}

	return type;
}

Bonobo_Embeddable
eog_embeddable_corba_object_create (BonoboObject *object)
{
	POA_Bonobo_Embeddable *servant;
	CORBA_Environment ev;
	
	servant = (POA_Bonobo_Embeddable *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_embeddable_vepv;

	CORBA_exception_init (&ev);
	POA_Bonobo_Embeddable__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Bonobo_Embeddable) bonobo_object_activate_servant (object, servant);
}

static const gchar *image_data_interfaces[] = {
	"IDL:Bonobo/ProgressiveDataSink:1.0",
	"IDL:Bonobo/PersistStream:1.0",
	"IDL:Bonobo/PersistFile:1.0",
	NULL
};

static BonoboView *
eog_embeddable_view_factory (BonoboEmbeddable *object, const Bonobo_ViewFrame view_frame,
			     void *closure)
{
	EogEmbeddable *embeddable;
	EogEmbeddableView *view;

	g_return_val_if_fail (object != NULL, NULL);
	g_return_val_if_fail (EOG_IS_EMBEDDABLE (object), NULL);

	embeddable = EOG_EMBEDDABLE (object);

	view = eog_embeddable_view_new (embeddable->priv->image_data);

	return BONOBO_VIEW (view);
}

static void
eog_embeddable_add_interfaces (EogEmbeddable *embeddable, BonoboObject *query_this,
			       const gchar **interfaces)
{
	const gchar **ptr;

	for (ptr = interfaces; *ptr; ptr++) {
		BonoboObject *object;

		object = bonobo_object_query_local_interface (query_this, *ptr);
		if (object)
			bonobo_object_add_interface (BONOBO_OBJECT (embeddable), object);
	}
}

EogEmbeddable *
eog_embeddable_construct (EogEmbeddable *embeddable,
			  Bonobo_Embeddable corba_object,
			  EogImageData *image_data)
{
	BonoboEmbeddable *retval;

	g_return_val_if_fail (embeddable != NULL, NULL);
	g_return_val_if_fail (EOG_IS_EMBEDDABLE (embeddable), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (image_data != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (image_data), NULL);

	embeddable->priv->image_data = image_data;
	bonobo_object_ref (BONOBO_OBJECT (image_data));

	eog_embeddable_add_interfaces (embeddable, BONOBO_OBJECT (embeddable->priv->image_data),
				       image_data_interfaces);

	retval = bonobo_embeddable_construct (BONOBO_EMBEDDABLE (embeddable), corba_object,
					      eog_embeddable_view_factory, NULL);

	if (retval == NULL)
		return NULL;

	return embeddable;
}

EogEmbeddable *
eog_embeddable_new (EogImageData *image_data)
{
	EogEmbeddable *embeddable;
	Bonobo_Embeddable corba_object;
	
	g_return_val_if_fail (image_data != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (image_data), NULL);

	embeddable = gtk_type_new (eog_embeddable_get_type ());

	corba_object = eog_embeddable_corba_object_create (BONOBO_OBJECT (embeddable));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (embeddable));
		return NULL;
	}
	
	return eog_embeddable_construct (embeddable, corba_object, image_data);
}
