/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * eog-image-view.c
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

#include <eog-image-view.h>

#include "preferences.h"

typedef struct {
	int type;
	const char *string;
} PrefsEntry;

typedef void (*PrefsChangeFunc) (EogImageView *, int);

typedef struct {
	PrefsChangeFunc cb_func;
	gpointer cb_data;
	int value;
} PrefsCbData;

/* Interpolation types for the index-mapping functions */
static PrefsEntry prefs_interpolation [] = {
	{ GNOME_EOG_INTERPOLATION_NEAREST,
	  N_("Nearest Neighbour Interpolation") },
	{ GNOME_EOG_INTERPOLATION_TILES,
	  N_("Tiles Interpolation") },
	{ GNOME_EOG_INTERPOLATION_BILINEAR,
	  N_("Bilinear Interpolation") },
	{ GNOME_EOG_INTERPOLATION_HYPERBOLIC,
	  N_("Hyperbolic Interpolation") },
	{ 0, NULL }
};

static PrefsEntry prefs_dither [] = {
	{ GNOME_EOG_DITHER_NONE,        N_("No dithering") },
	{ GNOME_EOG_DITHER_NORMAL,      N_("Normal (pseudocolor) dithering") },
	{ GNOME_EOG_DITHER_MAXIMUM,     N_("Maximum (high color) dithering") },
	{ 0, NULL }
};

static PrefsEntry prefs_check_type [] = {
	{ GNOME_EOG_CHECK_TYPE_DARK,    N_("Dark") },
	{ GNOME_EOG_CHECK_TYPE_MIDTONE, N_("Midtone") },
	{ GNOME_EOG_CHECK_TYPE_LIGHT,   N_("Light") },
	{ GNOME_EOG_CHECK_TYPE_BLACK,   N_("Black") },
	{ GNOME_EOG_CHECK_TYPE_GRAY,    N_("Gray") },
	{ GNOME_EOG_CHECK_TYPE_WHITE,   N_("White") },
	{ 0, NULL }
};

static PrefsEntry prefs_check_size [] = {
	{ GNOME_EOG_CHECK_SIZE_SMALL,   N_("Small") },
	{ GNOME_EOG_CHECK_SIZE_MEDIUM,  N_("Medium") },
	{ GNOME_EOG_CHECK_SIZE_LARGE,   N_("Large") },
	{ 0, NULL }
};

static void
activate_cb (GtkWidget *widget, PrefsCbData *cb_data)
{
	g_assert (EOG_IS_IMAGE_VIEW (cb_data->cb_data));

	cb_data->cb_func (cb_data->cb_data, cb_data->value);
}

static GtkWidget *
create_prefs_menu (const PrefsEntry *menu_description,
		   const int value, PrefsChangeFunc cb_func,
		   gpointer user_data)
{
	GtkWidget *omenu, *menu;
	const PrefsEntry *entry;
	GSList *group;
	int history = 0, i = 0;

	omenu = gtk_option_menu_new ();

	menu = gtk_menu_new ();
	group = NULL;
  
	for (entry = menu_description; entry->string; entry++) {
		const char *translated_string;
		PrefsCbData *cb_data = g_new0 (PrefsCbData, 1);
		GtkWidget *item;

		translated_string = gettext (entry->string);
		item = gtk_radio_menu_item_new_with_label (group, translated_string);
		group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (item));
		gtk_menu_append (GTK_MENU (menu), item);

		cb_data->value = i;
		cb_data->cb_func = cb_func;
		cb_data->cb_data = user_data;

		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    (GtkSignalFunc) activate_cb, cb_data);

		if (entry->type == value) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM (item), TRUE);
			history = i;
		}

		i++;

		gtk_widget_show (item);
	}
  
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), history);

	return omenu;
}

GtkWidget *
eog_create_preferences_page (EogImageView *image_view,
			     guint page_number)
{
	GtkWidget *table, *interp, *dither;
	GtkWidget *check_type, *check_size;

	g_return_val_if_fail (image_view != NULL, NULL);
	g_return_val_if_fail (EOG_IS_IMAGE_VIEW (image_view), NULL);
	g_return_val_if_fail (page_number == 0, NULL);

	table = gtk_table_new (4, 2, FALSE);

	interp = create_prefs_menu
		(prefs_interpolation,
		 eog_image_view_get_interpolation (image_view),
		 (PrefsChangeFunc) eog_image_view_set_interpolation,
		 image_view);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   gtk_label_new (_("Interpolation")),
				   0, 1, 0, 1);

	gtk_table_attach_defaults (GTK_TABLE (table), interp,
				   1, 2, 0, 1);

	dither = create_prefs_menu
		(prefs_dither,
		 eog_image_view_get_dither (image_view),
		 (PrefsChangeFunc) eog_image_view_set_dither, image_view);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   gtk_label_new (_("Dither")),
				   0, 1, 1, 2);

	gtk_table_attach_defaults (GTK_TABLE (table), dither,
				   1, 2, 1, 2);

	check_type = create_prefs_menu
		(prefs_check_type, eog_image_view_get_check_type (image_view),
		 (PrefsChangeFunc) eog_image_view_set_check_type, image_view);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   gtk_label_new (_("Check Type")),
				   0, 1, 2, 3);

	gtk_table_attach_defaults (GTK_TABLE (table), check_type,
				   1, 2, 2, 3);

	check_size = create_prefs_menu
		(prefs_check_size, eog_image_view_get_check_size (image_view),
		 (PrefsChangeFunc) eog_image_view_set_check_size, image_view);

	gtk_table_attach_defaults (GTK_TABLE (table),
				   gtk_label_new (_("Check Size")),
				   0, 1, 3, 4);

	gtk_table_attach_defaults (GTK_TABLE (table), check_size,
				   1, 2, 3, 4);

	return table;
}
