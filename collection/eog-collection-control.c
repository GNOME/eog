/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-collection-control.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000, SuSE GmbH.
 * Copyright 2001, The Free Software Foundation
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>

#include <bonobo/bonobo-macros.h>

#include "eog-collection-control.h"
#include "eog-collection-view.h"

struct _EogCollectionControlPrivate{
	EogCollectionView *view;
};

static void eog_collection_control_dispose (GObject *object);
static void eog_collection_control_finalize (GObject *object);
static void eog_collection_control_class_init (EogCollectionControlClass *klass);
static void eog_collection_control_instance_init (EogCollectionControl *object);

BONOBO_CLASS_BOILERPLATE (EogCollectionControl, eog_collection_control,
			  BonoboControl, BONOBO_TYPE_CONTROL);

static void
eog_collection_control_dispose (GObject *object)
{
	EogCollectionControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_CONTROL (object));

	control = EOG_COLLECTION_CONTROL (object);

	BONOBO_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_collection_control_finalize (GObject *object)
{
	EogCollectionControl *control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_CONTROL (object));

	control = EOG_COLLECTION_CONTROL (object);

	g_free (control->priv);

	BONOBO_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_collection_control_create_ui (EogCollectionControl *control)
{
	/* Currently we have no additional user interface. */
}

static void
eog_collection_control_set_ui_container (EogCollectionControl *eog_ctrl,
					 Bonobo_UIContainer ui_container)
{
	BonoboUIComponent *uic;

	g_return_if_fail (eog_ctrl != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_CONTROL (eog_ctrl));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);
	
	uic = bonobo_control_get_ui_component (BONOBO_CONTROL (eog_ctrl));

	eog_collection_view_set_ui_container (EOG_COLLECTION_VIEW (eog_ctrl->priv->view),
					      ui_container);

	bonobo_ui_component_set_container (uic, ui_container, NULL);
	eog_collection_control_create_ui (eog_ctrl);
}

static void
eog_collection_control_unset_ui_container (EogCollectionControl *eog_ctrl)
{
	 BonoboUIComponent *uic;

	g_return_if_fail (eog_ctrl != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_CONTROL (eog_ctrl));

	uic = bonobo_control_get_ui_component (BONOBO_CONTROL (eog_ctrl));

	eog_collection_view_unset_ui_container (EOG_COLLECTION_VIEW (eog_ctrl->priv->view));

	bonobo_ui_component_unset_container (uic, NULL);
}

static void
eog_collection_control_activate (BonoboControl *bctrl, gboolean state, gpointer data)
{
	EogCollectionControl *eog_ctrl;

	g_return_if_fail (bctrl != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_CONTROL (bctrl));

	eog_ctrl = EOG_COLLECTION_CONTROL (bctrl);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (bctrl), NULL);
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_collection_control_set_ui_container (eog_ctrl, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else
		eog_collection_control_unset_ui_container (eog_ctrl);

	BONOBO_CALL_PARENT (BONOBO_CONTROL_CLASS, activate, (bctrl, state));
}

static void
eog_collection_control_class_init (EogCollectionControlClass *klass)
{
	GObjectClass *object_class = (GObjectClass *)klass;

	object_class->dispose = eog_collection_control_dispose;
	object_class->finalize = eog_collection_control_finalize;
}

static void
eog_collection_control_instance_init (EogCollectionControl *control)
{
	control->priv = g_new0 (EogCollectionControlPrivate, 1);
}

static void
handle_open_uri (GObject *obj, gchar *uri, gpointer data)
{
	EogCollectionControl *eog_ctrl;
	BonoboObject *bctrl;
	CORBA_Environment ev;
	Bonobo_ControlFrame ctrl_frame;

	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_CONTROL (data));

	eog_ctrl = EOG_COLLECTION_CONTROL (data);
	CORBA_exception_init (&ev);
	
	bctrl = bonobo_object_query_local_interface (BONOBO_OBJECT (eog_ctrl),
						     "IDL:Bonobo/Control:1.0");
	if (bctrl == NULL) {
		g_error (_("Couldn't get local BonoboControl interface.\n"));
		return;
	}

	ctrl_frame = bonobo_control_get_control_frame (BONOBO_CONTROL (bctrl), NULL);
	Bonobo_ControlFrame_activateURI (ctrl_frame, CORBA_string_dup (uri), CORBA_FALSE, &ev);
	
	CORBA_Object_release (ctrl_frame, &ev);
	bonobo_object_unref (BONOBO_OBJECT (bctrl));
	CORBA_exception_free (&ev);
}

EogCollectionControl *
eog_collection_control_construct (EogCollectionControl    *eog_ctrl)
{
	EogCollectionControlPrivate *priv;
	BonoboControl     *bctrl;	
	BonoboPropertyBag *property_bag;
	GtkWidget *widget;

	g_return_val_if_fail (eog_ctrl != NULL, NULL);
	g_return_val_if_fail (EOG_IS_COLLECTION_CONTROL (eog_ctrl), NULL);

	priv = eog_ctrl->priv;

	/* construct parent object */
	priv->view =  EOG_COLLECTION_VIEW (eog_collection_view_new ());
	widget = eog_collection_view_get_widget (priv->view);
	bonobo_control_construct (BONOBO_CONTROL (eog_ctrl), widget);

	bonobo_object_add_interface (BONOBO_OBJECT (eog_ctrl),
				     BONOBO_OBJECT (priv->view));

	gtk_widget_unref (widget);

	g_signal_connect (eog_ctrl, "activate", 
			  G_CALLBACK (eog_collection_control_activate), NULL);

	/* add properties */
	property_bag = eog_collection_view_get_property_bag (priv->view);
	bonobo_control_set_properties (BONOBO_CONTROL (eog_ctrl), 
				       bonobo_object_corba_objref (BONOBO_OBJECT (property_bag)),
				       NULL);
	bonobo_object_unref (BONOBO_OBJECT (property_bag));

	/* connect collection view signals */
	g_signal_connect (G_OBJECT (priv->view),
			  "open_uri", 
			  G_CALLBACK (handle_open_uri), eog_ctrl);

	return eog_ctrl;
}

EogCollectionControl *
eog_collection_control_new (void)
{
	EogCollectionControl *control;

	control = g_object_new (EOG_TYPE_COLLECTION_CONTROL, NULL);

	return eog_collection_control_construct (control);
}
