/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-control.c
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

#include <eog-image-control.h>

struct _EogImageControlPrivate {
	EogImageData       *image_data;
	EogImageView       *image_view;

	BonoboPropertyBag  *property_bag;

	GtkWidget          *root;
};

POA_EOG_ImageControl__vepv eog_image_control_vepv;

static BonoboControlClass *eog_image_control_parent_class;

static void
init_eog_image_control_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_image_control_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_image_control_vepv.Bonobo_Control_epv = bonobo_control_get_epv ();
}

static void
eog_image_control_activate (BonoboControl *control, gboolean state)
{
	EogImageControl *image_control;

	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_IMAGE_CONTROL (control));

	image_control = EOG_IMAGE_CONTROL (control);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (image_control));
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_image_view_set_ui_container (image_control->priv->image_view, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else {
		eog_image_view_unset_ui_container (image_control->priv->image_view);
	}

	if (BONOBO_CONTROL_CLASS (eog_image_control_parent_class)->activate)
		BONOBO_CONTROL_CLASS (eog_image_control_parent_class)->activate (control, state);
}

static void
eog_image_control_destroy (GtkObject *object)
{
	EogImageControl *image_control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_CONTROL (object));

	image_control = EOG_IMAGE_CONTROL (object);

	if (image_control->priv->property_bag) {
		bonobo_object_unref (BONOBO_OBJECT (image_control->priv->property_bag));
		image_control->priv->property_bag = NULL;
	}

	if (image_control->priv->image_data) {
		bonobo_object_unref (BONOBO_OBJECT (image_control->priv->image_data));
		image_control->priv->image_data = NULL;
	}

	if (image_control->priv->image_view) {
		bonobo_object_unref (BONOBO_OBJECT (image_control->priv->image_view));
		image_control->priv->image_view = NULL;
	}

	if (image_control->priv->root) {
		gtk_widget_unref (image_control->priv->root);
		image_control->priv->root = NULL;
	}

	GTK_OBJECT_CLASS (eog_image_control_parent_class)->destroy (object);
}

static void
eog_image_control_finalize (GtkObject *object)
{
	EogImageControl *image_control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_IMAGE_CONTROL (object));

	image_control = EOG_IMAGE_CONTROL (object);

	g_free (image_control->priv);

	GTK_OBJECT_CLASS (eog_image_control_parent_class)->finalize (object);
}

static void
eog_image_control_class_init (EogImageControl *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	BonoboControlClass *control_class = (BonoboControlClass *)klass;

	eog_image_control_parent_class = gtk_type_class (bonobo_control_get_type ());

	object_class->destroy = eog_image_control_destroy;
	object_class->finalize = eog_image_control_finalize;

	control_class->activate = eog_image_control_activate;

	init_eog_image_control_corba_class ();
}

static void
eog_image_control_init (EogImageControl *image_control)
{
	image_control->priv = g_new0 (EogImageControlPrivate, 1);
}

GtkType
eog_image_control_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogImageControl",
			sizeof (EogImageControl),
			sizeof (EogImageControlClass),
			(GtkClassInitFunc) eog_image_control_class_init,
			(GtkObjectInitFunc) eog_image_control_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_control_get_type (), &info);
	}

	return type;
}

EOG_ImageControl
eog_image_control_corba_object_create (BonoboObject *object)
{
	POA_EOG_ImageControl *servant;
	CORBA_Environment ev;
	
	servant = (POA_EOG_ImageControl *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_image_control_vepv;

	CORBA_exception_init (&ev);
	POA_EOG_ImageControl__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (EOG_ImageControl) bonobo_object_activate_servant (object, servant);
}

static const gchar *image_data_interfaces[] = {
	"IDL:Bonobo/ProgressiveDataSink:1.0",
	"IDL:Bonobo/PersistStream:1.0",
	"IDL:Bonobo/PersistFile:1.0",
	NULL
};

static const gchar *image_view_interfaces[] = {
	"IDL:Bonobo/Zoomable:1.0",
	NULL
};

static void
eog_image_control_add_interfaces (EogImageControl *image_control, BonoboObject *query_this,
				  const gchar **interfaces)
{
	const gchar **ptr;

	for (ptr = interfaces; *ptr; ptr++) {
		BonoboObject *object;

		object = bonobo_object_query_local_interface (query_this, *ptr);
		if (object)
			bonobo_object_add_interface (BONOBO_OBJECT (image_control), object);
	}
}

EogImageControl *
eog_image_control_construct (EogImageControl *image_control, EOG_ImageControl corba_object,
			     EogImageData *image_data)
{
	BonoboControl *retval;

	g_return_val_if_fail (image_control != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_CONTROL (image_control), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (image_data != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (image_data), NULL);

	image_control->priv->image_data = image_data;
	bonobo_object_ref (BONOBO_OBJECT (image_data));

	image_control->priv->image_view = eog_image_view_new (image_data);
	image_control->priv->root = eog_image_view_get_widget (image_control->priv->image_view);

	eog_image_control_add_interfaces (image_control, BONOBO_OBJECT (image_control->priv->image_data),
					  image_data_interfaces);
	eog_image_control_add_interfaces (image_control, BONOBO_OBJECT (image_control->priv->image_view),
					  image_view_interfaces);

	retval = bonobo_control_construct (BONOBO_CONTROL (image_control), corba_object,
					   image_control->priv->root);
	if (retval == NULL)
		return NULL;

	image_control->priv->property_bag = eog_image_view_get_property_bag (image_control->priv->image_view);
	bonobo_control_set_property_bag (BONOBO_CONTROL (image_control), image_control->priv->property_bag);
	
	return image_control;
}

EogImageControl *
eog_image_control_new (EogImageData *image_data)
{
	EogImageControl *image_control;
	EOG_ImageControl corba_object;
	
	g_return_val_if_fail (image_data != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_DATA (image_data), NULL);

	image_control = gtk_type_new (eog_image_control_get_type ());

	corba_object = eog_image_control_corba_object_create (BONOBO_OBJECT (image_control));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (image_control));
		return NULL;
	}
	
	return eog_image_control_construct (image_control, corba_object, image_data);
}
