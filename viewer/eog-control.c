/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-control.c
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

#include <eog-control.h>

struct _EogControlPrivate {
	EogImage           *image;
	EogImageView       *image_view;

	BonoboPropertyBag  *property_bag;

	GtkWidget          *root;
};

POA_Bonobo_Control__vepv eog_control_vepv;

static BonoboControlClass *eog_control_parent_class;

static void
init_eog_control_corba_class (void)
{
	/* Setup the vector of epvs */
	eog_control_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	eog_control_vepv.Bonobo_Control_epv = bonobo_control_get_epv ();
}

static void
eog_control_activate (BonoboControl *object, gboolean state)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (control));
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_image_view_set_ui_container (control->priv->image_view, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else {
		eog_image_view_unset_ui_container (control->priv->image_view);
	}

	if (BONOBO_CONTROL_CLASS (eog_control_parent_class)->activate)
		BONOBO_CONTROL_CLASS (eog_control_parent_class)->activate (object, state);
}

static void
eog_control_destroy (GtkObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	if (control->priv->property_bag) {
		bonobo_object_unref (BONOBO_OBJECT (control->priv->property_bag));
		control->priv->property_bag = NULL;
	}

	if (control->priv->image) {
		bonobo_object_unref (BONOBO_OBJECT (control->priv->image));
		control->priv->image = NULL;
	}

	if (control->priv->image_view) {
		bonobo_object_unref (BONOBO_OBJECT (control->priv->image_view));
		control->priv->image_view = NULL;
	}

	if (control->priv->root) {
		gtk_widget_unref (control->priv->root);
		control->priv->root = NULL;
	}

	GTK_OBJECT_CLASS (eog_control_parent_class)->destroy (object);
}

static void
eog_control_finalize (GtkObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	g_free (control->priv);

	GTK_OBJECT_CLASS (eog_control_parent_class)->finalize (object);
}

static void
eog_control_class_init (EogControl *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	BonoboControlClass *control_class = (BonoboControlClass *)klass;

	eog_control_parent_class = gtk_type_class (bonobo_control_get_type ());

	object_class->destroy = eog_control_destroy;
	object_class->finalize = eog_control_finalize;

	control_class->activate = eog_control_activate;

	init_eog_control_corba_class ();
}

static void
eog_control_init (EogControl *control)
{
	control->priv = g_new0 (EogControlPrivate, 1);
}

GtkType
eog_control_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EogControl",
			sizeof (EogControl),
			sizeof (EogControlClass),
			(GtkClassInitFunc) eog_control_class_init,
			(GtkObjectInitFunc) eog_control_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_control_get_type (), &info);
	}

	return type;
}

Bonobo_Control
eog_control_corba_object_create (BonoboObject *object)
{
	POA_Bonobo_Control *servant;
	CORBA_Environment ev;
	
	servant = (POA_Bonobo_Control *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &eog_control_vepv;

	CORBA_exception_init (&ev);
	POA_Bonobo_Control__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Bonobo_Control) bonobo_object_activate_servant (object, servant);
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
eog_control_add_interfaces (EogControl *control, BonoboObject *query_this,
			    const gchar **interfaces)
{
	const gchar **ptr;

	for (ptr = interfaces; *ptr; ptr++) {
		BonoboObject *object;

		object = bonobo_object_query_local_interface (query_this, *ptr);
		if (object)
			bonobo_object_add_interface (BONOBO_OBJECT (control), object);
	}
}

EogControl *
eog_control_construct (EogControl *control, Bonobo_Control corba_object,
		       EogImage *image)
{
	BonoboControl *retval;

	g_return_val_if_fail (control != NULL, NULL);
	g_return_val_if_fail (EOG_IS_CONTROL (control), NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	control->priv->image = image;
	bonobo_object_ref (BONOBO_OBJECT (image));

	control->priv->image_view = eog_image_view_new (image);
	control->priv->root = eog_image_view_get_widget (control->priv->image_view);

	eog_control_add_interfaces (control,
				    BONOBO_OBJECT (control->priv->image),
				    image_data_interfaces);
	eog_control_add_interfaces (control,
				    BONOBO_OBJECT (control->priv->image_view),
				    image_view_interfaces);

	retval = bonobo_control_construct (BONOBO_CONTROL (control), corba_object,
					   control->priv->root);
	if (retval == NULL)
		return NULL;

	control->priv->property_bag = eog_image_view_get_property_bag (control->priv->image_view);
	bonobo_control_set_properties (BONOBO_CONTROL (control), control->priv->property_bag);
	
	return control;
}

EogControl *
eog_control_new (EogImage *image)
{
	EogControl *control;
	Bonobo_Control corba_object;
	
	g_return_val_if_fail (image != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	control = gtk_type_new (eog_control_get_type ());

	corba_object = eog_control_corba_object_create (BONOBO_OBJECT (control));
	if (corba_object == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}
	
	return eog_control_construct (control, corba_object, image);
}
