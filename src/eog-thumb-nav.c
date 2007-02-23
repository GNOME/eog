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

enum
{
	PROP_SHOW_BUTTONS = 1,
	PROP_THUMB_VIEW,
	PROP_MODE
};

struct _EogThumbNavPrivate {
	EogThumbNavMode   mode;

	gboolean          show_buttons;

	GtkWidget        *button_left;
	GtkWidget        *button_right;
	GtkWidget        *sw;
	GtkWidget        *scale;
	GtkWidget        *thumbview;
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

	case PROP_THUMB_VIEW:
		g_value_set_object (value, nav->priv->thumbview);
		break;

	case PROP_MODE:
		g_value_set_int (value, 
			eog_thumb_nav_get_mode (nav));
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

	case PROP_THUMB_VIEW:
		nav->priv->thumbview = 
			GTK_WIDGET (g_value_get_object (value));
		break;

	case PROP_MODE:
		eog_thumb_nav_set_mode (nav, 
			g_value_get_int (value));
		break;
	}
}

static GObject *
eog_thumb_nav_constructor (GType type,
			   guint n_construct_properties,
			   GObjectConstructParam *construct_params)
{
	GObject *object;
	EogThumbNavPrivate *priv;

	object = G_OBJECT_CLASS (eog_thumb_nav_parent_class)->constructor
			(type, n_construct_properties, construct_params);

	priv = EOG_THUMB_NAV (object)->priv;

	if (priv->thumbview != NULL) {
		gtk_container_add (GTK_CONTAINER (priv->sw), priv->thumbview);
		gtk_widget_show_all (priv->sw);
	}

	return object;
}

static void
eog_thumb_nav_class_init (EogThumbNavClass *class)
{
	GObjectClass *g_object_class = (GObjectClass *) class;

	g_object_class->constructor  = eog_thumb_nav_constructor;
	g_object_class->get_property = eog_thumb_nav_get_property;
	g_object_class->set_property = eog_thumb_nav_set_property;

	g_object_class_install_property (g_object_class,
	                                 PROP_SHOW_BUTTONS,
	                                 g_param_spec_boolean ("show-buttons",
	                                                       "Show Buttons",
	                                                       "Whether to show navigation buttons or not",
	                                                       TRUE,
	                                                       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property (g_object_class,
	                                 PROP_THUMB_VIEW,
	                                 g_param_spec_object ("thumbview",
	                                                       "Thumbnail View",
	                                                       "The internal thumbnail viewer widget",
	                                                       EOG_TYPE_THUMB_VIEW,
	                                                       (G_PARAM_CONSTRUCT_ONLY |
								G_PARAM_READABLE | 
								G_PARAM_WRITABLE)));

	g_object_class_install_property (g_object_class,
	                                 PROP_MODE,
	                                 g_param_spec_int ("mode",
	                                                   "Mode",
	                                                   "Thumb navigator mode",
	                                                   EOG_THUMB_NAV_MODE_ONE_ROW,
							   EOG_THUMB_NAV_MODE_MULTIPLE_ROWS,
							   EOG_THUMB_NAV_MODE_ONE_ROW,
	                                                   (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_type_class_add_private (g_object_class, sizeof (EogThumbNavPrivate));
}

static void
eog_thumb_nav_init (EogThumbNav *nav)
{
	EogThumbNavPrivate *priv;
	GtkAdjustment *adj;
	GtkWidget *arrow;

	nav->priv = EOG_THUMB_NAV_GET_PRIVATE (nav);

	priv = nav->priv;

	priv->mode = EOG_THUMB_NAV_MODE_ONE_ROW;

	priv->show_buttons = TRUE;

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

	priv->sw = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->sw), 
					     GTK_SHADOW_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
					GTK_POLICY_AUTOMATIC,
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

        gtk_box_pack_start (GTK_BOX (nav), priv->sw, TRUE, TRUE, 0);

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

	gtk_adjustment_value_changed (adj);
}

GtkWidget *
eog_thumb_nav_new (GtkWidget       *thumbview, 
		   EogThumbNavMode  mode, 
		   gboolean         show_buttons)
{
	GObject *nav;

	nav = g_object_new (EOG_TYPE_THUMB_NAV, 
		            "show-buttons", show_buttons,
		            "mode", mode,
		            "thumbview", thumbview,
		            "homogeneous", FALSE,
		            "spacing", 0,
			    NULL); 

	return GTK_WIDGET (nav);
}

gboolean
eog_thumb_nav_get_show_buttons (EogThumbNav *nav)
{
	g_return_val_if_fail (EOG_IS_THUMB_NAV (nav), FALSE);

	return nav->priv->show_buttons; 
}

void 
eog_thumb_nav_set_show_buttons (EogThumbNav *nav, gboolean show_buttons)
{
	g_return_if_fail (EOG_IS_THUMB_NAV (nav));
	g_return_if_fail (nav->priv->button_left  != NULL);
	g_return_if_fail (nav->priv->button_right != NULL);

	nav->priv->show_buttons = show_buttons;

	if (show_buttons && 
	    nav->priv->mode == EOG_THUMB_NAV_MODE_ONE_ROW) {
		gtk_widget_show_all (nav->priv->button_left);
		gtk_widget_show_all (nav->priv->button_right);
	} else {
		gtk_widget_hide_all (nav->priv->button_left);
		gtk_widget_hide_all (nav->priv->button_right);
	}
}

EogThumbNavMode
eog_thumb_nav_get_mode (EogThumbNav *nav)
{
	g_return_val_if_fail (EOG_IS_THUMB_NAV (nav), FALSE);

	return nav->priv->mode; 
}

void 
eog_thumb_nav_set_mode (EogThumbNav *nav, EogThumbNavMode mode)
{
	EogThumbNavPrivate *priv;

	g_return_if_fail (EOG_IS_THUMB_NAV (nav));

	priv = nav->priv;

	priv->mode = mode;

	switch (mode)
	{
	case EOG_THUMB_NAV_MODE_ONE_ROW:
		eog_thumb_nav_set_show_buttons (nav, priv->show_buttons);

		gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), 
					   G_MAXINT);

		gtk_widget_set_size_request (priv->thumbview, -1, 110);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_ALWAYS,
						GTK_POLICY_NEVER);

		break;

	case EOG_THUMB_NAV_MODE_ONE_COLUMN:
		gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), 1);

		gtk_widget_set_size_request (priv->thumbview, 110, -1);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_ALWAYS);

		gtk_widget_hide_all (priv->button_left);
		gtk_widget_hide_all (priv->button_right);
		
		break;

	case EOG_THUMB_NAV_MODE_MULTIPLE_ROWS:
		gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), -1);

		gtk_widget_set_size_request (priv->thumbview, -1, 220);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);

		gtk_widget_hide_all (priv->button_left);
		gtk_widget_hide_all (priv->button_right);
		
		break;
	}
}
