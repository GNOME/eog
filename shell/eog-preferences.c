/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-preferences.c
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

#include <eog-preferences.h>

struct _EogPreferencesPrivate {
	EogWindow          *window;
};

static GnomePropertyBoxClass *eog_preferences_parent_class;

static void
eog_preferences_destroy (GtkObject *object)
{
	EogPreferences *preferences;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_PREFERENCES (object));

	preferences = EOG_PREFERENCES (object);

	if (preferences->priv->window)
		gtk_object_unref (GTK_OBJECT (preferences->priv->window));
	preferences->priv->window = NULL;

	GTK_OBJECT_CLASS (eog_preferences_parent_class)->destroy (object);
}

static void
eog_preferences_finalize (GtkObject *object)
{
	EogPreferences *preferences;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_PREFERENCES (object));

	preferences = EOG_PREFERENCES (object);

	g_free (preferences->priv);

	GTK_OBJECT_CLASS (eog_preferences_parent_class)->finalize (object);
}

static void
eog_preferences_class_init (EogPreferences *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_preferences_parent_class = gtk_type_class (gnome_property_box_get_type ());

	object_class->destroy = eog_preferences_destroy;
	object_class->finalize = eog_preferences_finalize;
}

static void
eog_preferences_init (EogPreferences *preferences)
{
	preferences->priv = g_new0 (EogPreferencesPrivate, 1);
}

GtkType
eog_preferences_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"EogPreferences",
			sizeof (EogPreferences),
			sizeof (EogPreferencesClass),
			(GtkClassInitFunc)  eog_preferences_class_init,
			(GtkObjectInitFunc) eog_preferences_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gnome_property_box_get_type (), &info);
	}

	return type;
}

static void
add_property_control_page (EogPreferences *preferences,
			   Bonobo_PropertyControl property_control,
			   Bonobo_UIContainer uic,
			   CORBA_long page_num,
			   CORBA_Environment *ev)
{
	GtkWidget *control_widget;
	Bonobo_PropertyBag property_bag;
	Bonobo_Control control;
	gchar *title = NULL;

	control = Bonobo_PropertyControl_getControl (property_control,
						     page_num, ev);

	if (control == CORBA_OBJECT_NIL)
		return;

	control_widget = bonobo_widget_new_control_from_objref (control, uic);

	gtk_widget_show_all (control_widget);

	property_bag = Bonobo_Unknown_queryInterface
		(control, "IDL:Bonobo/PropertyBag:1.0", ev);

	if (property_bag != CORBA_OBJECT_NIL)
		title = bonobo_property_bag_client_get_value_string
			(property_bag, "bonobo:title", ev);
	else
		title = g_strdup ("Unknown");

	gnome_property_box_append_page (GNOME_PROPERTY_BOX (preferences),
					control_widget, gtk_label_new (title));
}

EogPreferences *
eog_preferences_construct (EogPreferences *preferences,
			   EogWindow      *window)
{
	Bonobo_PropertyControl prop_control;
	Bonobo_UIContainer uic;
	CORBA_Environment ev;
	CORBA_long page_count, i;

	g_return_val_if_fail (preferences != NULL, NULL);
	g_return_val_if_fail (window != NULL, NULL);
	g_return_val_if_fail (EOG_IS_PREFERENCES (preferences), NULL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	preferences->priv->window = window;
	gtk_object_ref (GTK_OBJECT (window));

	CORBA_exception_init (&ev);

	prop_control = eog_window_get_property_control (window, &ev);

	if (prop_control == CORBA_OBJECT_NIL) {
		gtk_object_destroy (GTK_OBJECT (preferences));
		CORBA_exception_free (&ev);
		return NULL;
	}

	uic = eog_window_get_ui_container (window, &ev);

	if (uic == CORBA_OBJECT_NIL) {
		gtk_object_destroy (GTK_OBJECT (preferences));
		CORBA_exception_free (&ev);
		return NULL;
	}

	page_count = Bonobo_PropertyControl__get_pageCount (prop_control, &ev);

	for (i = 0; i < page_count; i++)
	    add_property_control_page (preferences, prop_control,
				       uic, i, &ev);

	CORBA_exception_free (&ev);

	return preferences;
}

EogPreferences *
eog_preferences_new (EogWindow *window)
{
	EogPreferences *preferences;
	
	g_return_val_if_fail (window != NULL, NULL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	preferences = gtk_type_new (eog_preferences_get_type ());

	return eog_preferences_construct (preferences, window);
}
