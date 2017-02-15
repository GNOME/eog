/*
 * eog-zoom-entry.c
 * This file is part of eog
 *
 * Author: Felix Riemann <friemann@gnome.org>
 *
 * Copyright (C) 2017 GNOME Foundation
 *
 * Based on code (ev-zoom-action.c) by:
 *      - Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-zoom-entry.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>

enum {
	PROP_0,
	PROP_SCROLL_VIEW,
	PROP_MENU
};

typedef struct _EogZoomEntryPrivate {
	GtkWidget *btn_zoom_in;
	GtkWidget *btn_zoom_out;
	GtkWidget *value_entry;

	EogScrollView *view;

	GMenu *menu;
	GMenuModel *zoom_free_section;
	GtkWidget       *popup;
	gboolean         popup_shown;
} EogZoomEntryPrivate;

struct _EogZoomEntry {
	GtkBox box;

	EogZoomEntryPrivate *priv;
};

static const struct {
	const gchar *name;
	const double level;
} zoom_levels[] = {
	{ N_("33%"), (1.0/3.0) },
	{ N_("50%"), (1.0/2.0) },
	{ N_("100%"), 1.0 },
	{ N_("133%"), (1.0/0.75) },
	{ N_("200%"), 2.0 },
	{ N_("500%"), 5.0 },
	{ N_("1000%"), 10.0 },
	{ N_("1500%"), 15.0 },
	{ N_("2000%"), 20.0 }
};

G_DEFINE_TYPE_WITH_PRIVATE (EogZoomEntry, eog_zoom_entry, GTK_TYPE_BOX);

//static guint signals[LAST_SIGNAL] = { 0 };

static void eog_zoom_entry_reset_zoom_level (EogZoomEntry *entry);
static void eog_zoom_entry_set_zoom_level (EogZoomEntry *entry, gdouble zoom);


static void
eog_zoom_entry_populate_free_zoom_section (EogZoomEntry *zoom_entry)
{
	guint   i;
	gint    width = 0;

	for (i = 0; i < G_N_ELEMENTS (zoom_levels); i++) {
		GMenuItem *item;
		gint       length;

		if (zoom_levels[i].level > EOG_SCROLL_VIEW_MAX_ZOOM_FACTOR)
			break;

		length = g_utf8_strlen (zoom_levels[i].name, -1);
		if (length > width)
			width = length;

		item = g_menu_item_new (zoom_levels[i].name, NULL);
		g_menu_item_set_action_and_target (item, "win.zoom-set",
		                                   "d", zoom_levels[i].level);
		g_menu_append_item (G_MENU (zoom_entry->priv->zoom_free_section), item);
		g_object_unref (item);
	}
}

static void
eog_zoom_entry_activate_cb (GtkEntry *gtk_entry, EogZoomEntry *entry)
{
	const gchar *text = gtk_entry_get_text (gtk_entry);
	gchar *end_ptr = NULL;
	double zoom_perc;

	if (!text || text[0] == '\0') {
		eog_zoom_entry_reset_zoom_level (entry);
		return;
	}

	zoom_perc = g_strtod (text, &end_ptr);
	if (end_ptr && end_ptr[0] != '\0' && end_ptr[0] != '%') {
		eog_zoom_entry_reset_zoom_level (entry);
		return;
	}

	eog_scroll_view_set_zoom (entry->priv->view, zoom_perc / 100.0);
}

static gboolean
focus_out_cb (EogZoomEntry *zoom_entry)
{
	eog_zoom_entry_reset_zoom_level (zoom_entry);

	return FALSE;
}

static void
popup_menu_closed (GtkWidget    *popup,
                   EogZoomEntry *zoom_entry)
{
	if (zoom_entry->priv->popup != popup)
		return;

	zoom_entry->priv->popup_shown = FALSE;
	zoom_entry->priv->popup = NULL;
}

static GtkWidget*
get_popup (EogZoomEntry *zoom_entry)
{
	GdkRectangle rect;

	if (zoom_entry->priv->popup)
		return zoom_entry->priv->popup;

	zoom_entry->priv->popup = gtk_popover_new_from_model (GTK_WIDGET (zoom_entry),
	                                                       G_MENU_MODEL (zoom_entry->priv->menu));
	g_signal_connect (zoom_entry->priv->popup, "closed",
	                  G_CALLBACK (popup_menu_closed),
	                  zoom_entry);
	gtk_entry_get_icon_area (GTK_ENTRY (zoom_entry->priv->value_entry),
	                         GTK_ENTRY_ICON_SECONDARY, &rect);
	gtk_popover_set_relative_to (GTK_POPOVER (zoom_entry->priv->popup),
	                             zoom_entry->priv->value_entry);
	gtk_popover_set_pointing_to (GTK_POPOVER (zoom_entry->priv->popup), &rect);
	gtk_popover_set_position (GTK_POPOVER (zoom_entry->priv->popup), GTK_POS_BOTTOM);
	gtk_widget_set_size_request (zoom_entry->priv->popup, 150, -1);

	return zoom_entry->priv->popup;
}

static void
eog_zoom_entry_icon_press_cb (GtkEntry *entry, GtkEntryIconPosition icon_pos,
                              GdkEvent *event, gpointer data)
{
	EogZoomEntry *zoom_entry;
	guint button;

	g_return_if_fail (EOG_IS_ZOOM_ENTRY (data));
	g_return_if_fail (icon_pos == GTK_ENTRY_ICON_SECONDARY);

	if (!gdk_event_get_button (event, &button) || button != GDK_BUTTON_PRIMARY)
		return;

	zoom_entry = EOG_ZOOM_ENTRY (data);

	gtk_widget_show (get_popup (zoom_entry));
	zoom_entry->priv->popup_shown = TRUE;
}

static void
eog_zoom_entry_view_zoom_changed_cb (EogScrollView *view, gdouble zoom,
                                     gpointer data)
{
	EogZoomEntry *zoom_entry = EOG_ZOOM_ENTRY (data);

	eog_zoom_entry_set_zoom_level (zoom_entry, zoom);
}

static void
eog_zoom_entry_constructed (GObject *object)
{
	EogZoomEntry *zoom_entry = EOG_ZOOM_ENTRY (object);

	G_OBJECT_CLASS (eog_zoom_entry_parent_class)->constructed (object);

	g_signal_connect (zoom_entry->priv->view,
	                  "zoom-changed",
	                  G_CALLBACK (eog_zoom_entry_view_zoom_changed_cb),
	                  zoom_entry);
	eog_zoom_entry_reset_zoom_level (zoom_entry);

	zoom_entry->priv->zoom_free_section =
	                g_menu_model_get_item_link (G_MENU_MODEL (zoom_entry->priv->menu),
	                                            1, G_MENU_LINK_SECTION);
	eog_zoom_entry_populate_free_zoom_section (zoom_entry);
}

static void
eog_zoom_entry_finalize (GObject *object)
{
	EogZoomEntry *zoom_entry = EOG_ZOOM_ENTRY (object);

	g_clear_object (&zoom_entry->priv->menu);
	g_clear_object (&zoom_entry->priv->zoom_free_section);
	g_clear_object (&zoom_entry->priv->view);

	G_OBJECT_CLASS (eog_zoom_entry_parent_class)->finalize (object);
}

static void
eog_zoom_entry_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	EogZoomEntry *zoom_entry = EOG_ZOOM_ENTRY (object);

	switch (prop_id) {
	case PROP_SCROLL_VIEW:
		zoom_entry->priv->view = g_value_dup_object (value);
		break;
	case PROP_MENU:
		zoom_entry->priv->menu = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
eog_zoom_entry_set_zoom_level (EogZoomEntry *entry, gdouble zoom)
{
	gchar *zoom_str;
	gdouble zoom_percent;

	/* Mimic the zoom calculation from EogWindow to get matching displays */
	zoom = CLAMP (zoom, EOG_SCROLL_VIEW_MIN_ZOOM_FACTOR,
	              EOG_SCROLL_VIEW_MAX_ZOOM_FACTOR);
	zoom_percent = floor (zoom * 100. + 0.5);
	zoom_str = g_strdup_printf ("%d%%", (gint) zoom_percent);
	gtk_entry_set_text (GTK_ENTRY (entry->priv->value_entry), zoom_str);
	g_free (zoom_str);
}

static void
eog_zoom_entry_reset_zoom_level (EogZoomEntry *entry)
{
	const gdouble zoom = eog_scroll_view_get_zoom (entry->priv->view);
	eog_zoom_entry_set_zoom_level (entry, zoom);
}

static void
eog_zoom_entry_class_init (EogZoomEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *wklass = GTK_WIDGET_CLASS (klass);

	object_class->constructed = eog_zoom_entry_constructed;
	object_class->set_property = eog_zoom_entry_set_property;
	object_class->finalize = eog_zoom_entry_finalize;

	gtk_widget_class_set_template_from_resource (wklass,
	                                             "/org/gnome/eog/ui/eog-zoom-entry.ui");
	gtk_widget_class_bind_template_child_private (wklass,
	                                              EogZoomEntry,
	                                              btn_zoom_in);
	gtk_widget_class_bind_template_child_private (wklass,
	                                              EogZoomEntry,
	                                              btn_zoom_out);
	gtk_widget_class_bind_template_child_private (wklass,
	                                              EogZoomEntry,
	                                              value_entry);

	gtk_widget_class_bind_template_callback (wklass,
	                                         eog_zoom_entry_activate_cb);
	gtk_widget_class_bind_template_callback (wklass,
	                                         eog_zoom_entry_icon_press_cb);

	g_object_class_install_property (object_class, PROP_SCROLL_VIEW,
	                                 g_param_spec_object ("scroll-view",
	                                                      "EogScrollView",
	                                                      "The EogScrollView to work with",
	                                                      EOG_TYPE_SCROLL_VIEW,
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_MENU,
	                                 g_param_spec_object ("menu",
	                                                      "Menu",
	                                                      "The zoom popup menu",
	                                                      G_TYPE_MENU,
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));
}

static void
eog_zoom_entry_init (EogZoomEntry *entry)
{
	entry->priv = eog_zoom_entry_get_instance_private (entry);
	gtk_widget_init_template (GTK_WIDGET (entry));

	g_signal_connect_swapped (entry->priv->value_entry, "focus-out-event",
	                          G_CALLBACK (focus_out_cb),
	                          entry);
}

GtkWidget* eog_zoom_entry_new(EogScrollView *view, GMenu *menu)
{
	g_return_val_if_fail (EOG_IS_SCROLL_VIEW (view), NULL);
	g_return_val_if_fail (G_IS_MENU (menu), NULL);

	return g_object_new (EOG_TYPE_ZOOM_ENTRY,
	                     "scroll-view", view,
	                     "menu", menu,
	                     NULL);
}
