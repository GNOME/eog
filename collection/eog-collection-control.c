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

#include <gnome.h>
#include <bonobo.h>

#include "eog-collection-control.h"
#include "eog-collection-view.h"

struct _EogControlPrivate {
	GtkWidget         *root;

	BonoboUIComponent *uic;
};

#define PARENT_TYPE EOG_COLLECTION_VIEW_TYPE

static EogCollectionViewClass *eog_control_parent_class;

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
eog_control_set_ui_container (EogControl *eog_ctrl,
			      Bonobo_UIContainer ui_container)
{
	g_return_if_fail (eog_ctrl != NULL);
	g_return_if_fail (EOG_IS_CONTROL (eog_ctrl));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);
	
	eog_collection_view_set_ui_container (EOG_COLLECTION_VIEW (eog_ctrl),
					      ui_container);

	bonobo_ui_component_set_container (eog_ctrl->priv->uic, ui_container);

	eog_control_create_ui (eog_ctrl);
}

static void
eog_control_unset_ui_container (EogControl *eog_ctrl)
{
	g_return_if_fail (eog_ctrl != NULL);
	g_return_if_fail (EOG_IS_CONTROL (eog_ctrl));

	eog_collection_view_unset_ui_container (EOG_COLLECTION_VIEW (eog_ctrl));

	bonobo_ui_component_unset_container (eog_ctrl->priv->uic);
}

static void
eog_control_activate (BonoboControl *bctrl, gboolean state, gpointer data)
{
	EogControl *eog_ctrl;

	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_CONTROL (data));
	g_return_if_fail (bctrl != NULL);
	g_return_if_fail (BONOBO_IS_CONTROL (bctrl));

	eog_ctrl = EOG_CONTROL (data);

	if (state) {
		Bonobo_UIContainer ui_container;

		ui_container = bonobo_control_get_remote_ui_container (BONOBO_CONTROL (bctrl));
		if (ui_container != CORBA_OBJECT_NIL) {
			eog_control_set_ui_container (eog_ctrl, ui_container);
			bonobo_object_release_unref (ui_container, NULL);
		}
	} else
		eog_control_unset_ui_container (eog_ctrl);
}

static void
eog_control_class_init (EogControl *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_control_parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = eog_control_destroy;
	object_class->finalize = eog_control_finalize;
}

static void
eog_control_init (EogControl *control)
{
	control->priv = g_new0 (EogControlPrivate, 1);
}

BONOBO_X_TYPE_FUNC (EogControl,
		    PARENT_TYPE,
		    eog_control);

static void
handle_open_uri (GtkObject *obj, gchar *uri, gpointer data)
{
	EogControl *eog_ctrl;
	BonoboObject *bctrl;
	CORBA_Environment ev;
	Bonobo_ControlFrame ctrl_frame;

	g_return_if_fail (obj != NULL);
	g_return_if_fail (EOG_IS_CONTROL (obj));

	eog_ctrl = EOG_CONTROL (obj);
	CORBA_exception_init (&ev);
	
	bctrl = bonobo_object_query_local_interface (BONOBO_OBJECT (eog_ctrl),
						     "IDL:Bonobo/Control:1.0");
	if (bctrl == NULL) {
		g_error (_("Couldn't get local BonoboControl interface.\n"));
		return;
	}

	ctrl_frame = bonobo_control_get_control_frame (BONOBO_CONTROL (bctrl));
	Bonobo_ControlFrame_activateURI (ctrl_frame, CORBA_string_dup (uri), CORBA_FALSE, &ev);
	
	CORBA_exception_free (&ev);
}

EogControl *
eog_control_construct (EogControl    *eog_ctrl)
{
	BonoboControl     *bctrl;	

	g_return_val_if_fail (eog_ctrl != NULL, NULL);
	g_return_val_if_fail (EOG_IS_CONTROL (eog_ctrl), NULL);


	/* construct parent object */
	eog_collection_view_construct (EOG_COLLECTION_VIEW (eog_ctrl));

	eog_ctrl->priv->root = eog_collection_view_get_widget (EOG_COLLECTION_VIEW (eog_ctrl));
	

	/* add control interface */
	bctrl = bonobo_control_new (eog_ctrl->priv->root);
	bonobo_object_add_interface (BONOBO_OBJECT (eog_ctrl),
				     BONOBO_OBJECT (bctrl));

	eog_ctrl->priv->uic = bonobo_control_get_ui_component (bctrl);

	gtk_signal_connect (GTK_OBJECT (bctrl), "activate", 
			    eog_control_activate, eog_ctrl);


	/* connect collection view signals */
	gtk_signal_connect (GTK_OBJECT (eog_ctrl),
			    "open_uri", 
			    handle_open_uri, NULL);

	return eog_ctrl;
}

EogControl *
eog_control_new (void)
{
	EogControl *control;
	
	control = gtk_type_new (eog_control_get_type ());

	return eog_control_construct (control);
}
