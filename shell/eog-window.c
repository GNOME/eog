/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-window.c
 *
 * Authors:
 *   Federico Mena-Quintero (federico@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright (C) 2000-2001 The Free Software Foundation.
 * Copyright (C) 2001 SuSE GmbH.
 */
#include <config.h>

#include <eog-window.h>

struct _EogWindowPrivate {
	BonoboWindow       *app;

	GtkWidget          *box;
	GtkWidget          *control_widget;

	BonoboUIContainer  *ui_container;
	BonoboUIComponent  *uic;
};

static BonoboWindowClass *eog_window_parent_class;

static void
eog_window_destroy (GtkObject *object)
{
	EogWindow *window;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WINDOW (object));

	window = EOG_WINDOW (object);

	GTK_OBJECT_CLASS (eog_window_parent_class)->destroy (object);
}

static void
eog_window_finalize (GtkObject *object)
{
	EogWindow *window;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WINDOW (object));

	window = EOG_WINDOW (object);

	g_free (window->priv);

	GTK_OBJECT_CLASS (eog_window_parent_class)->finalize (object);
}

static void
eog_window_class_init (EogWindow *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;

	eog_window_parent_class = gtk_type_class (bonobo_window_get_type ());

	object_class->destroy = eog_window_destroy;
	object_class->finalize = eog_window_finalize;
}

static void
eog_window_init (EogWindow *window)
{
	window->priv = g_new0 (EogWindowPrivate, 1);
}

GtkType
eog_window_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"EogWindow",
			sizeof (EogWindow),
			sizeof (EogWindowClass),
			(GtkClassInitFunc)  eog_window_class_init,
			(GtkObjectInitFunc) eog_window_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (bonobo_window_get_type (), &info);
	}

	return type;
}

/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
}

static void
verb_FileExit_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogWindow *window;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	gtk_main_quit ();
}

static void
verb_HelpAbout_cb (BonoboUIComponent *uic, gpointer user_data, const char *cname)
{
	EogWindow *window;
	static GtkWidget *about;
	static const char *authors[] = {
		"Federico Mena-Quintero (federico@gnu.org)",
		"Arik Devens (arik@gnome.org)",
		"Martin Baulig (baulig@suse.de)",
		NULL
	};

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (EOG_IS_WINDOW (user_data));

	window = EOG_WINDOW (user_data);

	if (!about) {
		about = gnome_about_new (
			_("Eye of Gnome"),
			VERSION,
			_("Copyright (C) 2000 The Free Software Foundation"),
			authors,
			_("The GNOME image viewing and cataloging program"),
			NULL);
		gtk_signal_connect (GTK_OBJECT (about), "destroy",
				    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
				    &about);
	}

	gtk_widget_show_now (about);
	raise_and_focus (about);
}

static BonoboUIVerb eog_window_verbs[] = {
	BONOBO_UI_VERB ("FileExit",      verb_FileExit_cb),
	BONOBO_UI_VERB ("HelpAbout",     verb_HelpAbout_cb),
	BONOBO_UI_VERB_END
};

EogWindow *
eog_window_construct (EogWindow *window, const char *title)
{
	Bonobo_UIContainer corba_container;
	GtkWidget *retval;

	g_return_val_if_fail (window != NULL, NULL);
	g_return_val_if_fail (EOG_IS_WINDOW (window), NULL);

	retval = bonobo_window_construct (BONOBO_WINDOW (window),
					  "eog-shell", title);
	if (!retval)
		return NULL;

	window->priv->ui_container = bonobo_ui_container_new ();

	bonobo_ui_container_set_win (window->priv->ui_container,
				     BONOBO_WINDOW (window));

	gtk_window_set_default_size (GTK_WINDOW (window), 500, 440);
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, FALSE);

	window->priv->box = gtk_vbox_new (FALSE, 0);
	bonobo_window_set_contents (BONOBO_WINDOW (window),
				    window->priv->box);

	window->priv->uic = bonobo_ui_component_new ("eog");
	corba_container = BONOBO_OBJREF (window->priv->ui_container);
	bonobo_ui_component_set_container (window->priv->uic,
					   corba_container);

	bonobo_ui_util_set_ui (window->priv->uic, NULL, "eog-shell-ui.xml",
			       "eog-shell");

	bonobo_ui_component_add_verb_list_with_data
		(window->priv->uic, eog_window_verbs,
		 window);
	
	return window;
}

EogWindow *
eog_window_new (const char *title)
{
	EogWindow *window;
	
	window = gtk_type_new (eog_window_get_type ());

	return eog_window_construct (window, title);
}

void
eog_window_launch_component (EogWindow *window, const char *moniker)
{
	g_return_if_fail (window != NULL);
	g_return_if_fail (EOG_IS_WINDOW (window));
	g_return_if_fail (moniker != NULL);

	g_assert (window->priv->control_widget == NULL);

	window->priv->control_widget = bonobo_widget_new_control
		(moniker, BONOBO_OBJREF (window->priv->ui_container));

	g_assert (window->priv->control_widget != NULL);

	gtk_box_pack_start (GTK_BOX (window->priv->box),
			    window->priv->control_widget,
			    TRUE, TRUE, 0);
}
