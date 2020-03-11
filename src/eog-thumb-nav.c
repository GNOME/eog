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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
#include <math.h>
#include <string.h>

#define EOG_THUMB_NAV_SCROLL_INC      20
#define EOG_THUMB_NAV_SCROLL_MOVE     20
#define EOG_THUMB_NAV_SCROLL_TIMEOUT  20

enum
{
	PROP_0,
	PROP_SHOW_BUTTONS,
	PROP_THUMB_VIEW,
	PROP_MODE
};

struct _EogThumbNavPrivate {
	EogThumbNavMode   mode;
        gboolean          nav_horizontal;

	gboolean          show_buttons;
	gboolean          scroll_dir;
	gint              scroll_pos;
	gint              scroll_id;

	GtkWidget        *button_left_up;
	GtkWidget        *button_right_down;
	GtkWidget        *sw;
	GtkWidget        *thumbview;
	GtkAdjustment    *adj;

        gulong signal_adj_changed_id;
        gulong signal_adj_value_changed_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (EogThumbNav, eog_thumb_nav, GTK_TYPE_BOX);

static gboolean
eog_thumb_nav_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	EogThumbNav *nav = EOG_THUMB_NAV (user_data);
	gint inc = EOG_THUMB_NAV_SCROLL_INC * 3;

	if (nav->priv->mode != EOG_THUMB_NAV_MODE_ONE_ROW)
		return FALSE;

	switch (event->direction) {
	case GDK_SCROLL_UP:
	case GDK_SCROLL_LEFT:
		inc *= -1;
		break;

	case GDK_SCROLL_DOWN:
	case GDK_SCROLL_RIGHT:
		break;

	case GDK_SCROLL_SMOOTH:
	{
		/* Compatibility code to catch smooth events from mousewheels */
		gdouble x_delta, y_delta;
		gboolean set = gdk_event_get_scroll_deltas ((GdkEvent*)event,
		                                            &x_delta, &y_delta);

		/* Propagate horizontal smooth scroll events further,
		   as well as non-mousewheel events. */
		if (G_UNLIKELY (!set) || x_delta != 0.0 || fabs(y_delta) != 1.0)
			return FALSE;

		/* The y_delta is either +1.0 or -1.0 here */
		inc *= (gint) y_delta;
	}
	break;

	default:
		g_assert_not_reached ();
		return FALSE;
	}

	if (inc < 0)
		gtk_adjustment_set_value (nav->priv->adj, MAX (0, gtk_adjustment_get_value (nav->priv->adj) + inc));
	else
		gtk_adjustment_set_value (nav->priv->adj, MIN (gtk_adjustment_get_upper (nav->priv->adj) - gtk_adjustment_get_page_size (nav->priv->adj), gtk_adjustment_get_value (nav->priv->adj) + inc));

	return TRUE;
}

static void
eog_thumb_nav_adj_changed (GtkAdjustment *adj, gpointer user_data)
{
	EogThumbNav *nav;
	EogThumbNavPrivate *priv;
	gboolean ltr;

	nav = EOG_THUMB_NAV (user_data);
	priv = eog_thumb_nav_get_instance_private (nav);
	ltr = gtk_widget_get_direction (priv->sw) == GTK_TEXT_DIR_LTR;

	gtk_widget_set_sensitive (ltr ? priv->button_right_down : priv->button_left_up,
				  gtk_adjustment_get_value (adj)
				   < gtk_adjustment_get_upper (adj)
				    - gtk_adjustment_get_page_size (adj));
}

static void
eog_thumb_nav_adj_value_changed (GtkAdjustment *adj, gpointer user_data)
{
	EogThumbNav *nav;
	EogThumbNavPrivate *priv;
	gboolean ltr;

	nav = EOG_THUMB_NAV (user_data);
	priv = eog_thumb_nav_get_instance_private (nav);
	ltr = gtk_widget_get_direction (priv->sw) == GTK_TEXT_DIR_LTR;

	gtk_widget_set_sensitive (ltr ? priv->button_left_up : priv->button_right_down,
				  gtk_adjustment_get_value (adj) > 0);

	gtk_widget_set_sensitive (ltr ? priv->button_right_down : priv->button_left_up,
				  gtk_adjustment_get_value (adj)
				   < gtk_adjustment_get_upper (adj)
				    - gtk_adjustment_get_page_size (adj));
}

static gboolean
eog_thumb_nav_scroll_step (gpointer user_data)
{
	EogThumbNav *nav = EOG_THUMB_NAV (user_data);
	GtkAdjustment *adj = nav->priv->adj;
	gint delta;

	if (nav->priv->scroll_pos < 10)
		delta = EOG_THUMB_NAV_SCROLL_INC;
	else if (nav->priv->scroll_pos < 20)
		delta = EOG_THUMB_NAV_SCROLL_INC * 2;
	else if (nav->priv->scroll_pos < 30)
		delta = EOG_THUMB_NAV_SCROLL_INC * 2 + 5;
	else
		delta = EOG_THUMB_NAV_SCROLL_INC * 2 + 12;

	if (!nav->priv->scroll_dir)
		delta *= -1;

	if ((gint) (gtk_adjustment_get_value (adj) + (gdouble) delta) >= 0 &&
	    (gint) (gtk_adjustment_get_value (adj) + (gdouble) delta) <= gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj)) {
		gtk_adjustment_set_value(adj,
			gtk_adjustment_get_value (adj) + (gdouble) delta);
		nav->priv->scroll_pos++;
	} else {
		if (delta > 0)
		      gtk_adjustment_set_value (adj,
		      	gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj));
		else
		      gtk_adjustment_set_value (adj, 0);

		nav->priv->scroll_pos = 0;

		return FALSE;
	}

	return TRUE;
}

static void
eog_thumb_nav_button_clicked (GtkButton *button, EogThumbNav *nav)
{
	nav->priv->scroll_pos = 0;

	nav->priv->scroll_dir = gtk_widget_get_direction (GTK_WIDGET (button)) == GTK_TEXT_DIR_LTR ?
		GTK_WIDGET (button) == nav->priv->button_right_down :
		GTK_WIDGET (button) == nav->priv->button_left_up;

	eog_thumb_nav_scroll_step (nav);
}

static void
eog_thumb_nav_start_scroll (GtkButton *button, EogThumbNav *nav)
{
	nav->priv->scroll_dir = gtk_widget_get_direction (GTK_WIDGET (button)) == GTK_TEXT_DIR_LTR ?
		GTK_WIDGET (button) == nav->priv->button_right_down :
		GTK_WIDGET (button) == nav->priv->button_left_up;

	nav->priv->scroll_id = g_timeout_add (EOG_THUMB_NAV_SCROLL_TIMEOUT,
					      eog_thumb_nav_scroll_step,
					      nav);
}

static void
eog_thumb_nav_stop_scroll (GtkButton *button, EogThumbNav *nav)
{
	if (nav->priv->scroll_id > 0) {
		g_source_remove (nav->priv->scroll_id);
		nav->priv->scroll_id = 0;
		nav->priv->scroll_pos = 0;
	}
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
}

static void
eog_thumb_nav_init (EogThumbNav *nav)
{
	EogThumbNavPrivate *priv;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (nav),
					GTK_ORIENTATION_HORIZONTAL);

	nav->priv = eog_thumb_nav_get_instance_private (nav);

	priv = nav->priv;

	priv->mode = EOG_THUMB_NAV_MODE_ONE_ROW;

	priv->show_buttons = TRUE;

	priv->nav_horizontal = TRUE;

	priv->button_left_up = gtk_button_new_from_icon_name ("go-previous-symbolic",
							      GTK_ICON_SIZE_BUTTON);
	gtk_button_set_relief (GTK_BUTTON (priv->button_left_up), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (nav), priv->button_left_up, FALSE, FALSE, 0);

	g_signal_connect (priv->button_left_up,
			  "clicked",
			  G_CALLBACK (eog_thumb_nav_button_clicked),
			  nav);

	g_signal_connect (priv->button_left_up,
			  "pressed",
			  G_CALLBACK (eog_thumb_nav_start_scroll),
			  nav);

	g_signal_connect (priv->button_left_up,
			  "released",
			  G_CALLBACK (eog_thumb_nav_stop_scroll),
			  nav);

	priv->sw = gtk_scrolled_window_new (NULL, NULL);

	gtk_widget_set_name (gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (priv->sw)), "eog-image-gallery-scrollbar");

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->sw),
					     GTK_SHADOW_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_NEVER);

	g_signal_connect (priv->sw,
			  "scroll-event",
			  G_CALLBACK (eog_thumb_nav_scroll_event),
			  nav);
	gtk_widget_add_events (priv->sw, GDK_SCROLL_MASK);

	priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->sw));

	priv->signal_adj_changed_id = g_signal_connect (priv->adj,
							"changed",
							G_CALLBACK (eog_thumb_nav_adj_changed),
							nav);

	priv->signal_adj_value_changed_id = g_signal_connect (priv->adj,
							      "value-changed",
							      G_CALLBACK (eog_thumb_nav_adj_value_changed),
							      nav);

	gtk_box_pack_start (GTK_BOX (nav), priv->sw, TRUE, TRUE, 0);

	priv->button_right_down = gtk_button_new_from_icon_name ("go-next-symbolic",
							    GTK_ICON_SIZE_BUTTON);
	gtk_button_set_relief (GTK_BUTTON (priv->button_right_down), GTK_RELIEF_NONE);

	gtk_box_pack_start (GTK_BOX (nav), priv->button_right_down, FALSE, FALSE, 0);

	g_signal_connect (priv->button_right_down,
			  "clicked",
			  G_CALLBACK (eog_thumb_nav_button_clicked),
			  nav);

	g_signal_connect (priv->button_right_down,
			  "pressed",
			  G_CALLBACK (eog_thumb_nav_start_scroll),
			  nav);

	g_signal_connect (priv->button_right_down,
			  "released",
			  G_CALLBACK (eog_thumb_nav_stop_scroll),
			  nav);

	/* Update nav button states */
	eog_thumb_nav_adj_value_changed (priv->adj, nav);
}

static void
eog_thumb_nav_set_icon_buttons (EogThumbNav *nav, gboolean nav_horizontal)
{
	g_return_if_fail (EOG_IS_THUMB_NAV (nav));

	EogThumbNavPrivate *priv = nav->priv;

	if (!nav_horizontal) {
		GtkWidget *left_image = gtk_image_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (priv->button_left_up), left_image);

		GtkWidget *right_image = gtk_image_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (priv->button_right_down), right_image);
	} else {
		GtkWidget *up_image = gtk_image_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (priv->button_left_up), up_image);

		GtkWidget *down_image = gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (priv->button_right_down), down_image);
	}

	priv->nav_horizontal = nav_horizontal;

	return;
}

/**
 * eog_thumb_nav_new:
 * @thumbview: an #EogThumbView to embed in the navigation widget.
 * @mode: The navigation mode.
 * @show_buttons: Whether to show the navigation buttons.
 *
 * Creates a new thumbnail navigation widget.
 *
 * Returns: a new #EogThumbNav object.
 **/
GtkWidget *
eog_thumb_nav_new (GtkWidget       *thumbview,
		   EogThumbNavMode  mode,
		   gboolean         show_buttons)
{
	GObject *nav;

	nav = g_object_new (EOG_TYPE_THUMB_NAV,
			    "name", "eog-thumb-nav",
		            "show-buttons", show_buttons,
		            "mode", mode,
		            "thumbview", thumbview,
		            "homogeneous", FALSE,
		            "spacing", 0,
			    NULL);

	return GTK_WIDGET (nav);
}

/**
 * eog_thumb_nav_get_show_buttons:
 * @nav: an #EogThumbNav.
 *
 * Gets whether the navigation buttons are visible.
 *
 * Returns: %TRUE if the navigation buttons are visible,
 * %FALSE otherwise.
 **/
gboolean
eog_thumb_nav_get_show_buttons (EogThumbNav *nav)
{
	g_return_val_if_fail (EOG_IS_THUMB_NAV (nav), FALSE);

	return nav->priv->show_buttons;
}

/**
 * eog_thumb_nav_set_show_buttons:
 * @nav: an #EogThumbNav.
 * @show_buttons: %TRUE to show the buttons, %FALSE to hide them.
 *
 * Sets whether the navigation buttons to the left and right of the
 * widget should be visible.
 **/
void
eog_thumb_nav_set_show_buttons (EogThumbNav *nav, gboolean show_buttons)
{
	g_return_if_fail (EOG_IS_THUMB_NAV (nav));
	g_return_if_fail (nav->priv->button_left_up  != NULL);
	g_return_if_fail (nav->priv->button_right_down != NULL);

	nav->priv->show_buttons = show_buttons;

	if (show_buttons) {
		gtk_widget_show_all (nav->priv->button_left_up);
		gtk_widget_show_all (nav->priv->button_right_down);
	} else {
		gtk_widget_hide (nav->priv->button_left_up);
		gtk_widget_hide (nav->priv->button_right_down);
	}
}

/**
 * eog_thumb_nav_get_mode:
 * @nav: an #EogThumbNav.
 *
 * Gets the navigation mode in @nav.
 *
 * Returns: A value in #EogThumbNavMode.
 **/
EogThumbNavMode
eog_thumb_nav_get_mode (EogThumbNav *nav)
{
	g_return_val_if_fail (EOG_IS_THUMB_NAV (nav), FALSE);

	return nav->priv->mode;
}

/**
 * eog_thumb_nav_set_mode:
 * @nav: An #EogThumbNav.
 * @mode: One of #EogThumbNavMode.
 *
 * Sets the navigation mode in @nav. See #EogThumbNavMode for details.
 **/
void
eog_thumb_nav_set_mode (EogThumbNav *nav, EogThumbNavMode mode)
{
	EogThumbNavPrivate *priv;

	g_return_if_fail (EOG_IS_THUMB_NAV (nav));

	priv = nav->priv;

	if (priv->mode == mode)
	  return;

	priv->mode = mode;

	if (priv->signal_adj_changed_id != 0)
	  g_signal_handler_disconnect (priv->adj, priv->signal_adj_changed_id);
	if (priv->signal_adj_value_changed_id != 0)
	  g_signal_handler_disconnect (priv->adj, priv->signal_adj_value_changed_id);

	if (mode != EOG_THUMB_NAV_MODE_ONE_ROW) {
		gtk_orientable_set_orientation (GTK_ORIENTABLE(nav),
						GTK_ORIENTATION_VERTICAL);
		priv->adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->sw));
		eog_thumb_nav_set_icon_buttons (nav, TRUE);
	} else {
		gtk_orientable_set_orientation (GTK_ORIENTABLE(nav),
						GTK_ORIENTATION_HORIZONTAL);
		priv->adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->sw));
		eog_thumb_nav_set_icon_buttons (nav, FALSE);
	}

	priv->signal_adj_changed_id = g_signal_connect (priv->adj, "changed",
							G_CALLBACK (eog_thumb_nav_adj_changed),
							nav);
	priv->signal_adj_value_changed_id = g_signal_connect (priv->adj, "value-changed",
							      G_CALLBACK (eog_thumb_nav_adj_value_changed),
							      nav);

	switch (mode)
	{
	case EOG_THUMB_NAV_MODE_ONE_ROW:
		gtk_orientable_set_orientation (GTK_ORIENTABLE(priv->thumbview),
		                                GTK_ORIENTATION_HORIZONTAL);

		gtk_widget_set_size_request (priv->thumbview, -1, -1);
		eog_thumb_view_set_item_height (EOG_THUMB_VIEW (priv->thumbview),
						115);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_NEVER);

		eog_thumb_nav_set_show_buttons (nav, priv->show_buttons);

		break;

	case EOG_THUMB_NAV_MODE_ONE_COLUMN:
		gtk_orientable_set_orientation (GTK_ORIENTABLE(priv->thumbview),
		                                GTK_ORIENTATION_VERTICAL);
		gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), 1);

		gtk_widget_set_size_request (priv->thumbview, -1, -1);
		eog_thumb_view_set_item_height (EOG_THUMB_VIEW (priv->thumbview),
						-1);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);

		break;

	case EOG_THUMB_NAV_MODE_MULTIPLE_ROWS:
		gtk_orientable_set_orientation (GTK_ORIENTABLE(priv->thumbview),
		                                GTK_ORIENTATION_VERTICAL);
		gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), -1);

		gtk_widget_set_size_request (priv->thumbview, -1, 220);
		eog_thumb_view_set_item_height (EOG_THUMB_VIEW (priv->thumbview),
						-1);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);

		break;

	case EOG_THUMB_NAV_MODE_MULTIPLE_COLUMNS:
		gtk_orientable_set_orientation (GTK_ORIENTABLE(priv->thumbview),
		                                GTK_ORIENTATION_VERTICAL);
		gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->thumbview), -1);

		gtk_widget_set_size_request (priv->thumbview, 230, -1);
		eog_thumb_view_set_item_height (EOG_THUMB_VIEW (priv->thumbview),
						-1);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);

		break;
	}
}
