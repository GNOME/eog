/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-collection-control.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000 SuSE GmbH.
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>

#include <gnome.h>

#include "eog-collection-control.h"
#include "eog-collection-view.h"

struct _EogControlPrivate {
	EogCollectionView  *list_view;

	GtkWidget         *root;

	BonoboUIComponent *uic;
};

static BonoboControlClass *eog_control_parent_class;

static void
eog_control_destroy (GtkObject *object)
{
	EogControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_CONTROL (object));

	control = EOG_CONTROL (object);

	if (control->priv->root)
		gtk_widget_unref (control->priv->root);
	control->priv->root = NULL;

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
eog_control_create_ui (EogControl *control)
{
	/* Currently we have no additional user interface. */
}

static void
eog_control_set_ui_container (EogControl *control,
			      Bonobo_UIContainer ui_container)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);
	
	eog_collection_view_set_ui_container (control->priv->list_view,
					      ui_container);

	bonobo_ui_component_set_container (control->priv->uic, ui_container);

	eog_control_create_ui (control);
}

static void
eog_control_unset_ui_container (EogControl *control)
{
	g_return_if_fail (control != NULL);
	g_return_if_fail (EOG_IS_CONTROL (control));

	eog_collection_view_unset_ui_container (control->priv->list_view);

	bonobo_ui_component_unset_container (control->priv->uic);
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
			eog_control_set_ui_container (control, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else
		eog_control_unset_ui_container (control);

	if (BONOBO_CONTROL_CLASS (eog_control_parent_class)->activate)
		BONOBO_CONTROL_CLASS (eog_control_parent_class)->activate (object, state);
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

	if (!type) {
		GtkTypeInfo info = {
			"EogImageListControl",
			sizeof (EogControl),
			sizeof (EogControlClass),
			(GtkClassInitFunc)  eog_control_class_init,
			(GtkObjectInitFunc) eog_control_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (
			bonobo_control_get_type (), &info);
	}

	return type;
}

EogControl *
eog_control_construct (EogControl    *control)
{
	BonoboControl     *retval;

	g_return_val_if_fail (control != NULL, NULL);
	g_return_val_if_fail (EOG_IS_CONTROL (control), NULL);

	control->priv->list_view = eog_collection_view_new ();
	control->priv->root = eog_collection_view_get_widget (control->priv->list_view);
	if (!control->priv->list_view) {
		bonobo_object_unref (BONOBO_OBJECT (control));
		return NULL;
	}
	bonobo_object_add_interface (BONOBO_OBJECT (control),
				     BONOBO_OBJECT (control->priv->list_view));

	retval = bonobo_control_construct (BONOBO_CONTROL (control),
					   control->priv->root);
	if (!retval)
		return NULL;

	control->priv->uic = bonobo_control_get_ui_component (
		BONOBO_CONTROL (control));
	
	return control;
}

EogControl *
eog_control_new (void)
{
	EogControl *control;
	
	control = gtk_type_new (eog_control_get_type ());

	return eog_control_construct (control);
}
