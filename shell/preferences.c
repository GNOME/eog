/* Eye of Gnome image viewer - preferences
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
#include "preferences.h"

GdkInterpType  prefs_interp_type;
CheckType      prefs_check_type;
CheckSize      prefs_check_size;
GdkRgbDither   prefs_dither;
ScrollType     prefs_scroll;
GtkPolicyType  prefs_window_sb_policy;
gboolean       prefs_window_auto_size;
gboolean       prefs_open_new_window;
GtkPolicyType  prefs_full_screen_sb_policy;
FullScreenZoom prefs_full_screen_zoom;
gboolean       prefs_full_screen_fit_standard;
gboolean       prefs_full_screen_bevel;

/**
 * prefs_init:
 * @void:
 *
 * Reads the default set of preferences.  Must be called prior to any of the
 * rest of the program running.
 **/
void
prefs_init (void)
{
  	GConfClient *client;

	client = gconf_client_new ();

	prefs_interp_type = gconf_client_get_int (
		client, "/apps/eog/view/interp_type",
		NULL); /* bilinear default=2 */
	prefs_check_type = gconf_client_get_int (
		client, "/apps/eog/view/check_type",
		NULL); /* midtone default=1 */
	prefs_check_size = gconf_client_get_int (
		client, "/apps/eog/view/check_size",
		NULL); /* large default=2 */
	prefs_dither = gconf_client_get_int (
		client, "/apps/eog/view/dither",
		NULL); /* normal default=1 */
	prefs_scroll = gconf_client_get_int (
		client, "/apps/eog/view/scroll",
		NULL); /* two-pass default=1 */
	prefs_scroll = SCROLL_TWO_PASS;
	prefs_interp_type = GDK_INTERP_BILINEAR;

	prefs_window_sb_policy = gconf_client_get_int (
		client, "/apps/eog/window/sb_policy",
		NULL); /* automatic default=1 */
	prefs_window_auto_size = gconf_client_get_bool (
		client, "/apps/eog/window/auto_size",
		NULL); /* true default=1 */
	prefs_open_new_window = gconf_client_get_bool (
		client, "/apps/eog/window/open_new_window",
		NULL); /* false default=0 */

	prefs_full_screen_sb_policy = gconf_client_get_int (
		client, "/apps/eog/full_screen/sb_policy",
		NULL); /* never default=0 */
	prefs_full_screen_zoom = gconf_client_get_int (
		client, "/apps/eog/full_screen/zoom",
		NULL); /* fit default=0 */
	prefs_full_screen_fit_standard = gconf_client_get_bool (
  		client, "/apps/eog/full_screen/fit_standard",
  		NULL); /* true default=1 */
	prefs_full_screen_bevel = gconf_client_get_bool (
  		client, "/apps/eog/full_screen/bevel",
  		NULL); /* false default=0 */

	gtk_object_unref (GTK_OBJECT (client));
}

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
static GSList    *p_full_screen_zoom;
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

	p_full_screen_zoom = gtk_radio_button_group (GTK_RADIO_BUTTON (p_full_screen_zoom_radio));
	g_assert (p_full_screen_zoom != NULL);

	gtk_window_set_title (GTK_WINDOW (p_dialog), _("Preferences"));

	return TRUE;
}

/* Returns the index of the active item in an option menu.  GTK+ sucks for not
 * providing this already!
 */
static int
option_menu_get_active (GtkOptionMenu *omenu)
{
	GtkMenu *menu;
	GtkMenuItem *item;
	GList *l;
	int i;

	menu = GTK_MENU (gtk_option_menu_get_menu (omenu));
	item = GTK_MENU_ITEM (gtk_menu_get_active (menu));

	l = GTK_MENU_SHELL (menu)->children;

	for (i = 0; l; l = l->next) {
		if (GTK_MENU_ITEM (l->data) == item)
			return i;

		i++;
	}

	return -1;
}

/* Converts an enumeration value to the appropriate index in an item group.  The
 * enumeration values for the items are provided as a -1-terminated array.
 */
static int
enum_to_index (const int *enum_vals, int enum_val)
{
	int i;

	for (i = 0; enum_vals[i] != -1; i++)
		if (enum_vals[i] == enum_val)
			return i;

	return -1;
}

/* Converts an index in an item group to the appropriate enum value.  See the
 * function above.
 */
static int
index_to_enum (const int *enum_vals, int index)
{
	int i;

	/* We do this the hard way, i.e. not as a simple array reference, to
	 * check for correctness.
	 */

	for (i = 0; enum_vals[i] != -1; i++)
		if (i == index)
			return enum_vals[i];

	return -1;
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

#define omenu_set(omenu, enum_map, enum_val)	\
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), enum_to_index ((enum_map), (enum_val)));

#define toggle_set(toggle, val)		\
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), (val));

/* Sets the active item in a radio group according to the index-mapping tables */
static void
radio_set (GSList *group, const int *enum_vals, int enum_val)
{
	int i;
	GSList *l;

	i = enum_to_index (enum_vals, enum_val);
	g_return_if_fail (i != -1);

	l = g_slist_nth (group, i);
	g_return_if_fail (l != NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (l->data), TRUE);
}

/* Sets the current preferences values to the widgets in the dialog */
static void
set_prefs_widgets (void)
{
	omenu_set (p_interp_type, interp_types, prefs_interp_type);
	omenu_set (p_check_type, check_types, prefs_check_type);
	omenu_set (p_check_size, check_sizes, prefs_check_size);
	omenu_set (p_dither, dither_types, prefs_dither);
	toggle_set (p_scroll, (prefs_scroll == SCROLL_NORMAL) ? FALSE : TRUE);

	omenu_set (p_window_sb_policy, sb_policies, prefs_window_sb_policy);
	toggle_set (p_window_auto_size, prefs_window_auto_size);
	toggle_set (p_open_new_window, prefs_open_new_window);

	omenu_set (p_full_screen_sb_policy, sb_policies, prefs_full_screen_sb_policy);
	radio_set (p_full_screen_zoom, full_screen_zooms, prefs_full_screen_zoom);
	toggle_set (p_full_screen_fit_standard, prefs_full_screen_fit_standard);
	toggle_set (p_full_screen_bevel, prefs_full_screen_bevel);
}

/* Callback used to tell the property box that some value changed */
static void
activate_cb (GtkWidget *widget, gpointer data)
{
	gnome_property_box_changed (GNOME_PROPERTY_BOX (p_dialog));
}

/* Hooks an option menu's items the property box */
static void
omenu_hook (GtkOptionMenu *omenu)
{
	GtkMenu *menu;
	GList *l;

	menu = GTK_MENU (gtk_option_menu_get_menu (omenu));

	for (l = GTK_MENU_SHELL (menu)->children; l; l = l->next)
		gtk_signal_connect (GTK_OBJECT (l->data), "activate",
				    GTK_SIGNAL_FUNC (activate_cb), NULL);
}

/* Callback used to tell the property box that some value changed */
static void
toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	/* For radio buttons, only notify the property box when they are active,
	 * since we'll get called once per item in the radio group.  Sigh.
	 */
	if (!GTK_IS_RADIO_BUTTON (toggle) || toggle->active)
		gnome_property_box_changed (GNOME_PROPERTY_BOX (p_dialog));
}

/* Hooks a toggle button to the property box */
static void
toggle_hook (GtkToggleButton *toggle)
{
	gtk_signal_connect (GTK_OBJECT (toggle), "toggled",
			    GTK_SIGNAL_FUNC (toggled_cb), NULL);
}

/* Hooks a radio group to the property box */
static void
radio_hook (GSList *group)
{
	GSList *l;

	for (l = group; l; l = l->next)
		toggle_hook (GTK_TOGGLE_BUTTON (l->data));
}

/* Hooks the widgets so that they notify the property box when they change */
static void
hook_prefs_widgets (void)
{
	omenu_hook (GTK_OPTION_MENU (p_interp_type));
	omenu_hook (GTK_OPTION_MENU (p_check_type));
	omenu_hook (GTK_OPTION_MENU (p_check_size));
	omenu_hook (GTK_OPTION_MENU (p_dither));
	toggle_hook (GTK_TOGGLE_BUTTON (p_scroll));

	omenu_hook (GTK_OPTION_MENU (p_window_sb_policy));
	toggle_hook (GTK_TOGGLE_BUTTON (p_window_auto_size));
	toggle_hook (GTK_TOGGLE_BUTTON (p_open_new_window));

	omenu_hook (GTK_OPTION_MENU (p_full_screen_sb_policy));
	radio_hook (p_full_screen_zoom);
	toggle_hook (GTK_TOGGLE_BUTTON (p_full_screen_fit_standard));
	toggle_hook (GTK_TOGGLE_BUTTON (p_full_screen_bevel));
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

	/* FIXME: run and get value */
	gnome_dialog_run (GNOME_DIALOG (p_dialog));

	gtk_object_unref (GTK_OBJECT (p_xml));
	p_xml = NULL;

#if 0
	gtk_widget_destroy (p_dialog);
#endif
	p_dialog = NULL;
}
