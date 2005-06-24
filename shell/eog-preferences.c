#include <config.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnomeui/libgnomeui.h>
#include <glib/gi18n.h>
#include "util.h"
#include "eog-preferences.h"
#include "eog-config-keys.h"
#include "eog-hig-dialog.h"

#ifdef G_OS_WIN32

#undef EOG_DATADIR
#define EOG_DATADIR eog_get_datadir ()

#endif

#define GCONF_OBJECT_KEY             "GCONF_KEY"
#define GCONF_OBJECT_VALUE           "GCONF_VALUE"
#define OBJECT_WIDGET           	 "OBJECT_WIDGET"

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
check_auto_advance_toggle_cb (GtkWidget *widget, gpointer data)
{
	char *key = NULL;
	GtkWidget *child = NULL;
	gboolean state = FALSE;

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
	if (key == NULL) return;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	gconf_client_set_bool (GCONF_CLIENT (data),
			       key, state,			       
			       NULL);
	
	child = g_object_get_data (G_OBJECT (widget), OBJECT_WIDGET);
	if (child != NULL) {
		g_object_set (child, "sensitive", state, NULL);
	}
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
color_change_cb (GtkWidget *widget, guint red, guint green, guint blue, guint a, gpointer data)
{
	char *key = NULL;
	char *value = NULL;
	char *ptr;
	
	value = g_strdup_printf ("#%2X%2X%2X",
			     red / 256,
			     green / 256,
			     blue / 256);
  
	for (ptr = value; *ptr; ptr++)
		if (*ptr == ' ')
			*ptr = '0';

	key = g_object_get_data (G_OBJECT (widget), GCONF_OBJECT_KEY);
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
help_cb (GtkWidget *widget, gpointer data)
{
	GError *error = NULL;

	gnome_help_display ("eog.xml", "eog-prefs", &error);

	if (error) {
		GtkWidget *dialog;

		dialog = eog_hig_dialog_new (NULL, GTK_STOCK_DIALOG_ERROR,
					     _("Could not display help for Eye of GNOME"),
					     error->message, TRUE);

		gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);

		g_signal_connect_swapped (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);

		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

void
eog_preferences_show (GConfClient *client)
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

	widget = glade_xml_get_widget (xml, "close_button");
	g_signal_connect_swapped (G_OBJECT (widget), "clicked", 
				  G_CALLBACK (gtk_widget_destroy), dlg);

	widget = glade_xml_get_widget (xml, "help_button");

	if (widget)
		g_signal_connect (G_OBJECT (widget), "clicked", 
				  G_CALLBACK (help_cb), dlg);

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
	widget = glade_xml_get_widget (xml, "colorpicker");
	if (gdk_color_parse (value, &color)) {
		gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (widget),
					    color.red,
					    color.green,
					    color.blue,
					    255);
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

	widget = glade_xml_get_widget (xml, "auto_advance_check");	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_AUTO_ADVANCE, NULL));
	g_object_set_data (G_OBJECT (widget), OBJECT_WIDGET, glade_xml_get_widget (xml, "seconds_hbox"));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_AUTO_ADVANCE);
	g_signal_connect (G_OBJECT (widget), 
			  "toggled", 
			  G_CALLBACK (check_auto_advance_toggle_cb), 
			  client);

	widget = glade_xml_get_widget (xml, "seconds_spin");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 
				   gconf_client_get_int (client, EOG_CONF_FULLSCREEN_SECONDS, NULL));
	g_object_set_data (G_OBJECT (widget), GCONF_OBJECT_KEY, EOG_CONF_FULLSCREEN_SECONDS);
	g_signal_connect (G_OBJECT (widget), 
			  "changed", 
			  G_CALLBACK (spin_button_changed_cb), 
			  client);
			  
	if (!gconf_client_get_bool (client, EOG_CONF_FULLSCREEN_AUTO_ADVANCE, NULL)) {
		widget = glade_xml_get_widget (xml, "seconds_hbox");
		g_object_set (widget, "sensitive", FALSE, NULL);
	}
}
