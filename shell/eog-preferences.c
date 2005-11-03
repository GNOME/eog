#include <config.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-help.h>
#include "util.h"
#include "eog-preferences.h"
#include "eog-config-keys.h"

#ifdef G_OS_WIN32

#undef EOG_DATADIR
#define EOG_DATADIR eog_get_datadir ()

#endif

#define GCONF_OBJECT_KEY             "GCONF_KEY"
#define GCONF_OBJECT_VALUE           "GCONF_VALUE"
#define OBJECT_WIDGET                "OBJECT_WIDGET"

static void
check_toggle_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	if (key == NULL) return;

	gconf_client_set_bool (GCONF_CLIENT (data),
			       key,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
			       NULL);
}

static void
spin_button_changed_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	if (key == NULL) return;

	gconf_client_set_int (GCONF_CLIENT (data),
			      key,
			      gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget)),
			      NULL);
}

static void
color_change_cb (GtkColorButton *button, gpointer data)
{
	GdkColor color;
	char *key = NULL;
	char *value = NULL;

	gtk_color_button_get_color (button, &color);

	value = g_strdup_printf ("#%2X%2X%2X",
				 color.red / 256,
				 color.green / 256,
				 color.blue / 256);

	key = g_object_get_data (G_OBJECT (button), GCONF_OBJECT_KEY);
	if (key == NULL || value == NULL) 
		return;

	gconf_client_set_string (GCONF_CLIENT (data),
				 key,
				 value,
				 NULL);
}

static void
radio_toggle_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;
	char *value = NULL;
	

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) 
	    return;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	value = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_VALUE);
	if (key == NULL || value == NULL) 
		return;

	gconf_client_set_string (GCONF_CLIENT (data),
				 key,
				 value,
				 NULL);
}

static void
dialog_response_cb (GtkDialog *dlg, gint res_id, gpointer data)
{
	GError *error = NULL;

	switch (res_id) {
	case GTK_RESPONSE_HELP:
		gnome_help_display ("eog.xml", "eog-prefs", &error);

		if (error) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL,
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 _("Could not display help for Eye of GNOME"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
								  error->message);

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			g_error_free (error);
		}
		break;

	default:
		gtk_widget_destroy (GTK_WIDGET (dlg));
	}
}

void
eog_preferences_show (GtkWindow *parent, GConfClient *client)
{
	GtkWidget *dlg;
	GladeXML  *xml;
	GtkWidget *widget;
	char *value, *filename;
	GdkColor color;

	filename = g_build_filename (EOG_DATADIR, "eog/glade/eog.glade", NULL);
	xml = glade_xml_new (filename, "Hig Preferences Dialog", "eog");
	g_free (filename);
	g_assert (xml != NULL);

	dlg = glade_xml_get_widget (xml, "Hig Preferences Dialog");

	g_signal_connect (G_OBJECT (dlg), "response",
			  G_CALLBACK (dialog_response_cb), dlg);

	/* interpolate flag */
	widget = glade_xml_get_widget (xml, "interpolate_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_VIEW_INTERPOLATE, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_INTERPOLATE);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_toggle_cb), 
			  client);

	/* Transparency radio group */
	widget = glade_xml_get_widget (xml, "color_radio");
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANSPARENCY);
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_VALUE, "COLOR");
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (radio_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "checkpattern_radio");
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANSPARENCY);
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_VALUE, "CHECK_PATTERN");
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (radio_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "background_radio");
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANSPARENCY);
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_VALUE, "NONE");
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (radio_toggle_cb), 
			  client);
	
	value = gconf_client_get_string (client, EOG_CONF_VIEW_TRANSPARENCY, NULL);
	if (g_strcasecmp (value, "COLOR") == 0) {
		widget = glade_xml_get_widget (xml, "color_radio");
	}
	else if (g_strcasecmp (value, "CHECK_PATTERN") == 0) {
		widget = glade_xml_get_widget (xml, "checkpattern_radio");
	}
	else {
		widget = glade_xml_get_widget (xml, "background_radio");
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	/* color picker */
	g_free (value);
	value = gconf_client_get_string (client, EOG_CONF_VIEW_TRANS_COLOR, NULL);
	widget = glade_xml_get_widget (xml, "colorbutton");
	if (gdk_color_parse (value, &color)) {
		gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color);
	}
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_VIEW_TRANS_COLOR);
	g_signal_connect (G_OBJECT (widget),
			  "color-set",
			  G_CALLBACK (color_change_cb),
			  client);

	/* slideshow page */
	widget = glade_xml_get_widget (xml, "upscale_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_UPSCALE, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_UPSCALE);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "loop_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_LOOP, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_LOOP);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "seconds_spin");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 
				   gconf_client_get_int (client, EOG_CONF_FULLSCREEN_SECONDS, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_SECONDS);
	g_signal_connect (G_OBJECT (widget), 
			  "changed", 
			  G_CALLBACK (spin_button_changed_cb), 
			  client);
			  
	gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	gtk_widget_show_all (dlg);
}
