/* Eye of Gnome image viewer - preferences
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gnu.org>
 *          Arik Devens <arik@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gconf/gconf-client.h>
#include <libgnome/libgnome.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-propertybox.h>
#include <glade/glade.h>
#include "e-dialog-widgets.h"
#include "preferences.h"



/* Glade definition and contents of the preferences dialog */

static GladeXML *p_xml;

static GtkWidget *p_dialog;
static GtkWidget *p_interp_type;
static GtkWidget *p_check_type;
static GtkWidget *p_check_size;
static GtkWidget *p_dither;
static GtkWidget *p_scroll;
static GtkWidget *p_window_sb_policy;
static GtkWidget *p_window_auto_size;
static GtkWidget *p_open_new_window;
static GtkWidget *p_full_screen_sb_policy;
static GtkWidget *p_full_screen_zoom_radio;
static GtkWidget *p_full_screen_fit_standard;
static GtkWidget *p_full_screen_bevel;



/* Brings attention to a window by raising it and giving it focus */
static void
raise_and_focus (GtkWidget *widget)
{
	g_assert (GTK_WIDGET_REALIZED (widget));
	gdk_window_show (widget->window);
	gtk_widget_grab_focus (widget);
}

/* Interpolation types for the index-mapping functions */
static const int interp_types[] = {
	GDK_INTERP_NEAREST,
	GDK_INTERP_BILINEAR,
	GDK_INTERP_HYPER,
	-1
};

/* Check types for the index-mapping functions */
static const int check_types[] = {
	CHECK_TYPE_DARK,
	CHECK_TYPE_MIDTONE,
	CHECK_TYPE_LIGHT,
	CHECK_TYPE_BLACK,
	CHECK_TYPE_GRAY,
	CHECK_TYPE_WHITE,
	-1
};

/* Check sizes for the index-mapping functions */
static const int check_sizes[] = {
	CHECK_SIZE_SMALL,
	CHECK_SIZE_MEDIUM,
	CHECK_SIZE_LARGE,
	-1
};

/* Dither types for the index-mapping functions */
static const int dither_types[] = {
	GDK_RGB_DITHER_NONE,
	GDK_RGB_DITHER_NORMAL,
	GDK_RGB_DITHER_MAX,
	-1
};

/* Scrolling types for the index-mapping functions */
static const int scroll_types[] = {
	SCROLL_NORMAL,
	SCROLL_TWO_PASS,
	-1
};

/* Scrollbar policies for the index-mapping functions */
static const int sb_policies[] = {
	GTK_POLICY_NEVER,
	GTK_POLICY_AUTOMATIC,
	-1
};

/* Full screen zoom modes for the index-mapping functions */
static const int full_screen_zooms[] = {
	FULL_SCREEN_ZOOM_1,
	FULL_SCREEN_ZOOM_SAME_AS_WINDOW,
	FULL_SCREEN_ZOOM_FIT,
	-1
};

/* Callback for the apply signal of the property box */
static void
apply_cb (GnomePropertyBox *pbox, gint page_num, gpointer data)
{
	GConfClient *client;

	if (page_num != -1)
		return;

	client = gconf_client_get_default();

	/* View options */

	gconf_client_set_int (client, "/apps/eog/view/interp_type",
			      e_dialog_option_menu_get (GTK_WIDGET (p_interp_type),
							interp_types), NULL);
	gconf_client_set_int (client, "/apps/eog/view/check_type",
			      e_dialog_option_menu_get (GTK_WIDGET (p_check_type),
							check_types), NULL);
	gconf_client_set_int (client, "/apps/eog/view/check_size",
			      e_dialog_option_menu_get (GTK_WIDGET (p_check_size),
							check_sizes), NULL);
	gconf_client_set_int (client, "/apps/eog/view/dither",
			      e_dialog_option_menu_get (GTK_WIDGET (p_dither),
							dither_types), NULL);
	gconf_client_set_int (client, "/apps/eog/view/scroll",
			      e_dialog_toggle_get (GTK_WIDGET (p_scroll)), NULL);

	/* Window options */

	gconf_client_set_int (client, "/apps/eog/window/sb_policy",
			      e_dialog_option_menu_get (GTK_WIDGET (p_window_sb_policy),
							sb_policies), NULL);
	gconf_client_set_bool (client, "/apps/eog/window/auto_size",
			       e_dialog_toggle_get (GTK_WIDGET (p_window_auto_size)), NULL);
	gconf_client_set_bool (client, "/apps/eog/window/open_new_window",
			       e_dialog_toggle_get (GTK_WIDGET (p_open_new_window)), NULL);

	/* Full screen options */

	gconf_client_set_int (client, "/apps/eog/full_screen/sb_policy",
			      e_dialog_option_menu_get (GTK_WIDGET (p_full_screen_sb_policy),
							sb_policies), NULL);
	gconf_client_set_int (client, "/apps/eog/full_screen/zoom",
			      e_dialog_radio_get (GTK_WIDGET (p_full_screen_zoom_radio),
						  full_screen_zooms), NULL);
	gconf_client_set_bool (client, "/apps/eog/full_screen/fit_standard",
			       e_dialog_toggle_get (GTK_WIDGET (p_full_screen_fit_standard)), NULL);
	gconf_client_set_bool (client, "/apps/eog/full_screen/bevel",
			       e_dialog_toggle_get (GTK_WIDGET (p_full_screen_bevel)), NULL);

	gtk_object_unref (GTK_OBJECT (client));
}

/* Loads the preferences dialog from the Glade file */
static gboolean
load_prefs_dialog (void)
{
	p_xml = glade_xml_new (EOG_GLADEDIR "/preferences-dialog.glade", NULL);
	if (!p_xml) {
		g_message ("load_prefs_dialog(): Could not load the Glade XML file!");
		return FALSE;
	}

	p_dialog = glade_xml_get_widget (p_xml, "preferences-dialog");
	p_interp_type = glade_xml_get_widget (p_xml, "interpolation-type");
	p_check_type = glade_xml_get_widget (p_xml, "transparency-type");
	p_check_size = glade_xml_get_widget (p_xml, "check-size");
	p_dither = glade_xml_get_widget (p_xml, "dither-type");
	p_scroll = glade_xml_get_widget (p_xml, "two-pass-scrolling");
	p_window_sb_policy = glade_xml_get_widget (p_xml, "window-scrollbars");
	p_window_auto_size = glade_xml_get_widget (p_xml, "window-auto-size");
	p_open_new_window = glade_xml_get_widget (p_xml, "open-new-window");
	p_full_screen_sb_policy = glade_xml_get_widget (p_xml, "full-screen-scrollbars");
	p_full_screen_zoom_radio = glade_xml_get_widget (p_xml, "full-screen-zoom-radio");
	p_full_screen_fit_standard = glade_xml_get_widget (p_xml, "full-screen-fit-standard");
	p_full_screen_bevel = glade_xml_get_widget (p_xml, "full-screen-bevel");

	if (!(p_dialog && p_interp_type && p_check_type && p_check_size && p_dither
	      && p_scroll && p_window_sb_policy && p_window_auto_size && p_open_new_window
	      && p_full_screen_sb_policy && p_full_screen_zoom_radio && p_full_screen_fit_standard
	      && p_full_screen_bevel)) {
		g_message ("load_prefs_dialog(): Could not find all widgets in Glade file!");

		gtk_object_unref (GTK_OBJECT (p_xml));
		p_xml = NULL;

		if (p_dialog) {
			gtk_widget_destroy (p_dialog);
			p_dialog = NULL;
		}

		return FALSE;
	}

	gtk_window_set_title (GTK_WINDOW (p_dialog), _("Preferences"));

	gtk_signal_connect (GTK_OBJECT (p_dialog), "apply",
			    GTK_SIGNAL_FUNC (apply_cb),
			    NULL);

	return TRUE;
}


/* Sets the current preferences values to the widgets in the dialog */
static void
set_prefs_widgets (void)
{
	GConfClient *client;

	client = gconf_client_get_default ();

	/* View options */

	e_dialog_option_menu_set (GTK_WIDGET (p_interp_type),
				  gconf_client_get_int (
					  client, "/apps/eog/view/interp_type",
					  NULL), interp_types);
	e_dialog_option_menu_set (GTK_WIDGET (p_check_type),
				  gconf_client_get_int (
					  client, "/apps/eog/view/check_type",
					  NULL), check_types);
	e_dialog_option_menu_set (GTK_WIDGET (p_check_size),
				  gconf_client_get_int (
					  client, "/apps/eog/view/check_size",
					  NULL), check_sizes);
	e_dialog_option_menu_set (GTK_WIDGET (p_dither),
				  gconf_client_get_int (
					  client, "/apps/eog/view/dither",
					  NULL), dither_types);
	e_dialog_toggle_set (GTK_WIDGET (p_scroll),
			     gconf_client_get_int (
				     client, "/apps/eog/view/scroll",
				     NULL));

	/* Window options */

	e_dialog_option_menu_set (GTK_WIDGET (p_window_sb_policy),
				  gconf_client_get_int (
					  client, "/apps/eog/window/sb_policy",
					  NULL), sb_policies);
	e_dialog_toggle_set (GTK_WIDGET (p_window_auto_size),
			     gconf_client_get_bool (
				     client, "/apps/eog/window/auto_size",
				     NULL));
	e_dialog_toggle_set (GTK_WIDGET (p_open_new_window),
			     gconf_client_get_bool (
				     client, "/apps/eog/window/open_new_window",
				     NULL));

	/* Full screen options */

	e_dialog_option_menu_set (GTK_WIDGET (p_full_screen_sb_policy),
				  gconf_client_get_int (
					  client, "/apps/eog/full_screen/sb_policy",
					  NULL), sb_policies);
	e_dialog_radio_set (GTK_WIDGET (p_full_screen_zoom_radio),
			    gconf_client_get_int (
				    client, "/apps/eog/full_screen/zoom",
				    NULL), full_screen_zooms);
	e_dialog_toggle_set (GTK_WIDGET (p_full_screen_fit_standard),
			     gconf_client_get_bool (
				     client, "/apps/eog/full_screen/fit_standard",
				     NULL));
	e_dialog_toggle_set (GTK_WIDGET (p_full_screen_bevel),
			     gconf_client_get_bool (
				     client, "/apps/eog/full_screen/bevel",
				     NULL));

	gtk_object_unref (GTK_OBJECT (client));
}

/* Hooks the widgets so that they notify the property box when they change */
static void
hook_prefs_widgets (void)
{
	/* View options */

	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_interp_type));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_check_type));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_check_size));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_dither));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_scroll));

	/* Window options */

	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_window_sb_policy));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_window_auto_size));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_open_new_window));

	/* Full screen options */

	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_full_screen_sb_policy));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_full_screen_zoom_radio));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_full_screen_fit_standard));
	e_dialog_widget_hook_property (GTK_WIDGET (p_dialog), GTK_WIDGET (p_full_screen_bevel));
}

/**
 * prefs_dialog:
 * @void:
 *
 * Runs the preferences dialog.
 **/
void
prefs_dialog (void)
{
	/* Bring up the dialog if it is already running */
	if (p_xml) {
		g_assert (p_dialog != NULL);
		raise_and_focus (p_dialog);
		return;
	}

	g_assert (p_dialog == NULL);

	if (!load_prefs_dialog ())
		return;

	set_prefs_widgets ();
	hook_prefs_widgets ();

	gnome_dialog_run (GNOME_DIALOG (p_dialog));

	gtk_object_unref (GTK_OBJECT (p_xml));
	p_xml = NULL;

	/* The stupid thing already destroyed itself, so we don't destroy it
         * ourselves.
	 */
	p_dialog = NULL;
}
