/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-preferences.c
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *   Jens Finke (jens@triq.net)
 *
 * Copyright: 2000 SuSE GmbH.
 *            2002 Free Software Foundation
 */
#include <config.h>
#include <stdio.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtknotebook.h>
#include <libgnome/gnome-macros.h>
#include <eog-preferences.h>


GNOME_CLASS_BOILERPLATE (EogPreferences,
			 eog_preferences,
			 GtkDialog,
			 GTK_TYPE_DIALOG);

struct _EogPreferencesPrivate {
	EogWindow          *window;
	
	GtkWidget          *notebook;
	
};

static void
eog_preferences_destroy (GtkObject *object)
{
	EogPreferences *preferences;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_PREFERENCES (object));

	if (getenv ("DEBUG_EOG"))
		g_message ("Destroying EogPreferences...");

	preferences = EOG_PREFERENCES (object);

	if (preferences->priv->window)
		g_object_unref (preferences->priv->window);
	preferences->priv->window = NULL;

	GNOME_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
eog_preferences_finalize (GObject *object)
{
	EogPreferences *preferences;

	preferences = EOG_PREFERENCES (object);

	g_free (preferences->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_preferences_class_init (EogPreferencesClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	GObjectClass *gobject_class = (GObjectClass *)klass;

	object_class->destroy = eog_preferences_destroy;
	gobject_class->finalize = eog_preferences_finalize;
}

static void
eog_preferences_instance_init (EogPreferences *preferences)
{
	preferences->priv = g_new0 (EogPreferencesPrivate, 1);
}

static void
add_property_control_page (EogPreferences *preferences,
			   Bonobo_PropertyControl property_control,
			   BonoboUIContainer *uic,
			   CORBA_long page_num,
			   CORBA_Environment *ev)
{
	GtkWidget *page;
	GtkWidget *label;
	Bonobo_PropertyBag property_bag;
	Bonobo_Control control;
	gchar *title = NULL;

	control = Bonobo_PropertyControl_getControl (property_control,
						     page_num, ev);
	if (control == CORBA_OBJECT_NIL)
		return;
	
	/* Get title for page */
	property_bag = Bonobo_Unknown_queryInterface (control, 
						      "IDL:Bonobo/PropertyBag:1.0", 
						      ev);
	if (property_bag != CORBA_OBJECT_NIL) {
		title = bonobo_pbclient_get_string (property_bag, 
						    "bonobo:title", ev);
		bonobo_object_release_unref (property_bag, NULL);
	} else
		title = g_strdup ("Unknown");
	label = gtk_label_new (title);
	g_free (title);

	/* Get content for page */
	page = bonobo_widget_new_control_from_objref (control, 
						      BONOBO_OBJREF (uic));
	gtk_widget_show_all (page);

	gtk_notebook_append_page (GTK_NOTEBOOK (preferences->priv->notebook),
				  page, label);

	bonobo_object_release_unref (control, ev);
}

EogPreferences *
eog_preferences_construct (EogPreferences *preferences,
			   EogWindow      *window)
{
	Bonobo_PropertyControl prop_control;
	BonoboUIContainer *uic;
	CORBA_Environment ev;
	CORBA_long page_count, i;

	g_return_val_if_fail (preferences != NULL, NULL);
	g_return_val_if_fail (window != NULL, NULL);
	g_return_val_if_fail (EOG_IS_PREFERENCES (preferences), NULL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	preferences->priv->window = window;
	g_object_ref (window);

	gtk_window_set_title (GTK_WINDOW (preferences), _("Eye of Gnome Preferences"));
	gtk_dialog_add_button (GTK_DIALOG (preferences), GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (preferences), FALSE);
	preferences->priv->notebook = gtk_notebook_new ();
	gtk_widget_show (preferences->priv->notebook);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (preferences)->vbox), 
			   preferences->priv->notebook);
	g_signal_connect (preferences, "response", 
			  G_CALLBACK (gtk_widget_destroy), 
			  preferences);

	CORBA_exception_init (&ev);

	prop_control = eog_window_get_property_control (window, &ev);

	if (prop_control == CORBA_OBJECT_NIL) {
		gtk_object_destroy (GTK_OBJECT (preferences));
		CORBA_exception_free (&ev);
		return NULL;
	}

	uic = bonobo_window_get_ui_container (BONOBO_WINDOW (window));

	if (uic == NULL) {
		gtk_object_destroy (GTK_OBJECT (preferences));
		bonobo_object_release_unref (prop_control, &ev);
		CORBA_exception_free (&ev);
		return NULL;
	}

	page_count = Bonobo_PropertyControl__get_pageCount (prop_control, &ev);

	for (i = 0; i < page_count; i++)
	    add_property_control_page (preferences, prop_control,
				       uic, i, &ev);

	bonobo_object_release_unref (prop_control, &ev);
	CORBA_exception_free (&ev);

	return preferences;
}

GtkWidget *
eog_preferences_new (EogWindow *window)
{
	EogPreferences *preferences;
	
	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	preferences = g_object_new (eog_preferences_get_type (), NULL);

	return GTK_WIDGET (eog_preferences_construct (preferences, window));
}
