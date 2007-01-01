/* Eye Of Gnome - Thumbnail Navigator
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eog-thumb-nav.h"
#include "eog-thumb-view.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <string.h>

#define EOG_THUMB_NAV_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_THUMB_NAV, EogThumbNavPrivate))

G_DEFINE_TYPE (EogThumbNav, eog_thumb_nav, GTK_TYPE_HBOX);

#define EOG_THUMB_NAV_SCROLL_INC     1
#define EOG_THUMB_NAV_SCROLL_MOVE    20
#define EOG_THUMB_NAV_SCROLL_TIMEOUT 20

struct _EogThumbNavPrivate {
	gboolean   show_buttons;
	GtkWidget *button_left;
	GtkWidget *button_right;
	GtkWidget *sw;
	GtkWidget *scale;
};

enum
{
	PROP_SHOW_BUTTONS = 1,
	PROP_SCROLLED_WINDOW
};

static void
eog_thumb_nav_adj_changed (GtkAdjustment *adj, gpointer user_data)
{
	EogThumbNav *nav;
	EogThumbNavPrivate *priv;
	gdouble upper, page_size;

	nav = EOG_THUMB_NAV (user_data);
	priv = EOG_THUMB_NAV_GET_PRIVATE (nav);

	g_object_get (G_OBJECT (adj),
		      "upper", &upper,
		      "page-size", &page_size,
		      NULL);

	gtk_widget_set_sensitive (priv->button_right, upper > page_size);

	if (upper == page_size) {
		gtk_widget_hide (priv->scale);
	} else {
		gtk_widget_show (priv->scale);
	}
}

static void
eog_thumb_nav_adj_value_changed (GtkAdjustment *adj, gpointer user_data)
{
	EogThumbNav *nav;
	EogThumbNavPrivate *priv;
	gdouble upper, page_size, value;

	nav = EOG_THUMB_NAV (user_data);
	priv = EOG_THUMB_NAV_GET_PRIVATE (nav);

	g_object_get (G_OBJECT (adj),
		      "upper", &upper,
		      "page-size", &page_size,
		      "value", &value,
		      NULL);

	gtk_widget_set_sensitive (priv->button_left, value > 0);

	gtk_widget_set_sensitive (priv->button_right, 
				  value < upper - page_size);
}

static gboolean
eog_thumb_nav_scroll_left (gpointer user_data)
{
	gdouble value, move;
	static gint i = 0;

	GtkAdjustment *adj = GTK_ADJUSTMENT (user_data);

	g_object_get (G_OBJECT (adj),
		      "value", &value,
		      NULL);

	if (i == EOG_THUMB_NAV_SCROLL_MOVE || 
	    value - EOG_THUMB_NAV_SCROLL_INC < 0) {
		i = 0;
		gtk_adjustment_value_changed (adj);
		return FALSE;
	} 

	i++;

	move = MIN (EOG_THUMB_NAV_SCROLL_MOVE, value);
	gtk_adjustment_set_value (adj, value - move);

	return TRUE;
}

static gboolean
eog_thumb_nav_scroll_right (gpointer user_data)
{
	gdouble upper, page_size, value, move;
	static gint i = 0;

	GtkAdjustment *adj = GTK_ADJUSTMENT (user_data);

	g_object_get (G_OBJECT (adj),
		      "upper", &upper,
		      "page-size", &page_size,
		      "value", &value,
		      NULL);

	if (i == EOG_THUMB_NAV_SCROLL_MOVE || 
	    value + EOG_THUMB_NAV_SCROLL_INC > upper - page_size) {
		i = 0;
		return FALSE;
	} 

	i++;

	move = MIN (EOG_THUMB_NAV_SCROLL_MOVE, upper - page_size - value);
	gtk_adjustment_set_value (adj, value + move);
	gtk_adjustment_value_changed (adj);

	return TRUE;
}

static void
eog_thumb_nav_left_button_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *sw = EOG_THUMB_NAV (user_data)->priv->sw;
	GtkAdjustment *adj = 
		gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (sw));
	
	g_timeout_add (EOG_THUMB_NAV_SCROLL_TIMEOUT, eog_thumb_nav_scroll_left, adj);
}

static void
eog_thumb_nav_right_button_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *sw = EOG_THUMB_NAV (user_data)->priv->sw;
	GtkAdjustment *adj = 
		gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (sw));
	
	g_timeout_add (EOG_THUMB_NAV_SCROLL_TIMEOUT, eog_thumb_nav_scroll_right, adj);
}

static void 
eog_thumb_nav_get_property (GObject    *object, 
			    guint       property_id, 
			    GValue     *value, 
			    GParamSpec *pspec)
{
	EogThumbNav *nav = EOG_THUMB_NAV (object);

	switch (property_id)
	{
		case PROP_SHOW_BUTTONS:
			g_value_set_boolean (value, 
				eog_thumb_nav_get_show_buttons (nav));
			break;
	}
}

static void 
eog_thumb_nav_set_property (GObject      *object, 
			    guint         property_id, 
			    const GValue *value, 
			    GParamSpec   *pspec)
{
	EogThumbNav *nav = EOG_THUMB_NAV (object);

	switch (property_id)
	{
		case PROP_SHOW_BUTTONS:
			eog_thumb_nav_set_show_buttons (nav, 
				g_value_get_boolean (value));
			break;
	}
}

static void
eog_thumb_nav_class_init (EogThumbNavClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;

	g_object_class->get_property = eog_thumb_nav_get_property;
	g_object_class->set_property = eog_thumb_nav_set_property;

	g_object_class_install_property (g_object_class,
	                                 PROP_SHOW_BUTTONS,
	                                 g_param_spec_boolean ("show-buttons",
	                                                       "Show Buttons",
	                                                       "Whether to show navigation buttons or not",
	                                                       TRUE,
	                                                       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_type_class_add_private (g_object_class, sizeof (EogThumbNavPrivate));
}

static void
eog_thumb_nav_init (EogThumbNav *nav)
{
	EogThumbNavPrivate *priv;
	GtkAdjustment *adj;
	GtkWidget *arrow;
	GtkWidget *vbox;

	nav->priv = EOG_THUMB_NAV_GET_PRIVATE (nav);

	priv = nav->priv;

	g_object_set (G_OBJECT (nav), 
		      "homogeneous", FALSE,
		      "spacing", 0, 
		      NULL);

	gtk_vbutton_box_set_layout_default (GTK_BUTTONBOX_START);

        priv->button_left = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->button_left), GTK_RELIEF_NONE);

	arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_ETCHED_IN); 
	gtk_container_add (GTK_CONTAINER (priv->button_left), arrow);

	gtk_widget_set_size_request (GTK_WIDGET (priv->button_left), 20, 0);

        gtk_box_pack_start (GTK_BOX (nav), priv->button_left, FALSE, FALSE, 0);

	g_signal_connect (priv->button_left, 
			  "clicked", 
			  G_CALLBACK (eog_thumb_nav_left_button_clicked), 
			  nav);

	vbox = gtk_vbox_new (FALSE, 0);

	priv->sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->sw), 
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);

	adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->sw));

	g_signal_connect (adj, 
			  "changed", 
			  G_CALLBACK (eog_thumb_nav_adj_changed), 
			  nav);

	g_signal_connect (adj, 
			  "value-changed", 
			  G_CALLBACK (eog_thumb_nav_adj_value_changed), 
			  nav);

	priv->scale = gtk_hscale_new (adj);

	gtk_scale_set_draw_value (GTK_SCALE (priv->scale), FALSE);

        gtk_box_pack_start (GTK_BOX (vbox), priv->sw, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), priv->scale, FALSE, TRUE, 0);

        gtk_box_pack_start (GTK_BOX (nav), vbox, TRUE, TRUE, 0);

        priv->button_right = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (priv->button_right), GTK_RELIEF_NONE);

	arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE); 
	gtk_container_add (GTK_CONTAINER (priv->button_right), arrow);

	gtk_widget_set_size_request (GTK_WIDGET (priv->button_right), 20, 0);

        gtk_box_pack_start (GTK_BOX (nav), priv->button_right, FALSE, FALSE, 0);

	g_signal_connect (priv->button_right, 
			  "clicked", 
			  G_CALLBACK (eog_thumb_nav_right_button_clicked), 
			  nav);
}

GtkWidget *
eog_thumb_nav_new ()
{
	GtkWidget *nav;

	nav = GTK_WIDGET (g_object_new (EOG_TYPE_THUMB_NAV, NULL));

	return nav;
}

gboolean
eog_thumb_nav_get_show_buttons (EogThumbNav *nav)
{
	g_return_val_if_fail (EOG_IS_THUMB_NAV (nav), FALSE);

	return nav->priv->show_buttons; 
}

void eog_thumb_nav_set_show_buttons (EogThumbNav *nav, gboolean show_buttons)
{
	g_return_if_fail (EOG_IS_THUMB_NAV (nav));

	nav->priv->show_buttons = show_buttons;
}

void eog_thumb_nav_set_thumb_view (EogThumbNav  *nav, 
				   EogThumbView *thumbview)
{
	GtkAdjustment *adj;

	g_return_if_fail (EOG_IS_THUMB_NAV (nav));
	g_return_if_fail (EOG_IS_THUMB_VIEW (thumbview));

	if (GTK_BIN (nav->priv->sw)->child != NULL) {
		gtk_widget_destroy (GTK_BIN (nav->priv->sw)->child);
	}

	gtk_container_add (GTK_CONTAINER (nav->priv->sw), 
			   GTK_WIDGET (thumbview));

        adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (nav->priv->sw));

	gtk_adjustment_value_changed (adj);
}
