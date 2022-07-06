/* Eye of Gnome - Side bar
 *
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2007 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on evince code (shell/ev-sidebar.c) by:
 * 	- Jonathan Blandford <jrb@alum.mit.edu>
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

#include <glib/gi18n.h>

#include "eog-sidebar.h"

enum {
	PROP_0,
	PROP_CURRENT_PAGE
};

enum {
	PAGE_COLUMN_TITLE,
	PAGE_COLUMN_MENU_ITEM,
	PAGE_COLUMN_MAIN_WIDGET,
	PAGE_COLUMN_NOTEBOOK_INDEX,
	PAGE_COLUMN_NUM_COLS
};

enum {
	SIGNAL_PAGE_ADDED,
	SIGNAL_PAGE_REMOVED,
	SIGNAL_LAST
};

static gint signals[SIGNAL_LAST];

struct _EogSidebarPrivate {
	GtkWidget *notebook;
	GtkWidget *list_box;
	GtkWidget *popover;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *arrow;

	GtkTreeModel *page_model;
};

G_DEFINE_TYPE_WITH_PRIVATE (EogSidebar, eog_sidebar, GTK_TYPE_BOX)

static void
eog_sidebar_destroy (GtkWidget *object)
{
	EogSidebar *eog_sidebar = EOG_SIDEBAR (object);

	if (eog_sidebar->priv->page_model) {
		g_object_unref (eog_sidebar->priv->page_model);
		eog_sidebar->priv->page_model = NULL;
	}

	(* GTK_WIDGET_CLASS (eog_sidebar_parent_class)->destroy) (object);
}

static void
eog_sidebar_select_page (EogSidebar *eog_sidebar, GtkTreeIter *iter)
{
	gchar *title;
	gint index;

	gtk_tree_model_get (eog_sidebar->priv->page_model, iter,
			    PAGE_COLUMN_TITLE, &title,
			    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
			    -1);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (eog_sidebar->priv->notebook), index);
	gtk_label_set_text (GTK_LABEL (eog_sidebar->priv->label), title);

	g_free (title);
}

void
eog_sidebar_set_page (EogSidebar   *eog_sidebar,
		     GtkWidget   *main_widget)
{
	GtkTreeIter iter;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first (eog_sidebar->priv->page_model, &iter);

	while (valid) {
		GtkWidget *widget;

		gtk_tree_model_get (eog_sidebar->priv->page_model, &iter,
				    PAGE_COLUMN_MAIN_WIDGET, &widget,
				    -1);

		if (widget == main_widget) {
			eog_sidebar_select_page (eog_sidebar, &iter);
			valid = FALSE;
		} else {
			valid = gtk_tree_model_iter_next (eog_sidebar->priv->page_model, &iter);
		}

		g_object_unref (widget);
	}

	g_object_notify (G_OBJECT (eog_sidebar), "current-page");
}

gint
eog_sidebar_get_page_nr (EogSidebar *eog_sidebar)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (eog_sidebar->priv->notebook);

	return gtk_notebook_get_current_page (notebook);
}

void
eog_sidebar_set_page_nr (EogSidebar   *eog_sidebar,
		         gint          index)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (eog_sidebar->priv->notebook);

	gtk_notebook_set_current_page (notebook, index);
}

static GtkWidget *
eog_sidebar_get_current_page (EogSidebar *sidebar)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (sidebar->priv->notebook);

	return gtk_notebook_get_nth_page
		(notebook, gtk_notebook_get_current_page (notebook));
}

static void
eog_sidebar_set_property (GObject     *object,
		         guint         prop_id,
		         const GValue *value,
		         GParamSpec   *pspec)
{
	EogSidebar *sidebar = EOG_SIDEBAR (object);

	switch (prop_id) {
	case PROP_CURRENT_PAGE:
		eog_sidebar_set_page (sidebar, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
eog_sidebar_get_property (GObject    *object,
		          guint       prop_id,
		          GValue     *value,
		          GParamSpec *pspec)
{
	EogSidebar *sidebar = EOG_SIDEBAR (object);

	switch (prop_id) {
	case PROP_CURRENT_PAGE:
		g_value_set_object (value, eog_sidebar_get_current_page (sidebar));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
eog_sidebar_class_init (EogSidebarClass *eog_sidebar_class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (eog_sidebar_class);
	widget_class = GTK_WIDGET_CLASS (eog_sidebar_class);

	widget_class->destroy = eog_sidebar_destroy;
	g_object_class->get_property = eog_sidebar_get_property;
	g_object_class->set_property = eog_sidebar_set_property;

	g_object_class_install_property (g_object_class,
					 PROP_CURRENT_PAGE,
					 g_param_spec_object ("current-page",
							      "Current page",
							      "The currently visible page",
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE));

	signals[SIGNAL_PAGE_ADDED] =
		g_signal_new ("page-added",
			      EOG_TYPE_SIDEBAR,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogSidebarClass, page_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);

	signals[SIGNAL_PAGE_REMOVED] =
		g_signal_new ("page-removed",
			      EOG_TYPE_SIDEBAR,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogSidebarClass, page_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
}

static void
eog_sidebar_close_clicked_cb (GtkWidget *widget,
 			      gpointer   user_data)
{
	EogSidebar *eog_sidebar = EOG_SIDEBAR (user_data);

	gtk_widget_hide (GTK_WIDGET (eog_sidebar));
}

static void
eog_sidebar_menu_item_activate_cb (GtkListBox    *list_box,
                                   GtkListBoxRow *row,
				   gpointer       user_data)
{
	EogSidebar *eog_sidebar = EOG_SIDEBAR (user_data);
	GtkTreeIter iter;
	GtkWidget *item;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first (eog_sidebar->priv->page_model, &iter);

	while (valid) {
		gtk_tree_model_get (eog_sidebar->priv->page_model, &iter,
				    PAGE_COLUMN_MENU_ITEM, &item,
				    -1);

		if (item == GTK_WIDGET (row)) {
			eog_sidebar_select_page (eog_sidebar, &iter);
			valid = FALSE;
		} else {
			valid = gtk_tree_model_iter_next (eog_sidebar->priv->page_model, &iter);
		}

		g_object_unref (item);
	}

	gtk_popover_popdown (GTK_POPOVER (eog_sidebar->priv->popover));

	g_object_notify (G_OBJECT (eog_sidebar), "current-page");
}

static void
eog_sidebar_update_arrow_visibility (EogSidebar *sidebar)
{
	EogSidebarPrivate *priv = sidebar->priv;
	const gint n_pages = eog_sidebar_get_n_pages (sidebar);

	gtk_widget_set_visible (GTK_WIDGET (priv->arrow),
				n_pages > 1);
}

static void
eog_sidebar_init (EogSidebar *eog_sidebar)
{
	GtkWidget *hbox;
	GtkWidget *close_button;
	GtkWidget *select_hbox;
	GtkWidget *select_button;
	GtkWidget *arrow;
	GtkWidget *image;

	eog_sidebar->priv = eog_sidebar_get_instance_private (eog_sidebar);

	gtk_style_context_add_class (
		gtk_widget_get_style_context (GTK_WIDGET (eog_sidebar)),
					      GTK_STYLE_CLASS_SIDEBAR);

	/* data model */
	eog_sidebar->priv->page_model = (GtkTreeModel *)
			gtk_list_store_new (PAGE_COLUMN_NUM_COLS,
					    G_TYPE_STRING,
					    GTK_TYPE_WIDGET,
					    GTK_TYPE_WIDGET,
					    G_TYPE_INT);

	/* top option menu */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	g_object_set (hbox, "border-width", 6, NULL);
	eog_sidebar->priv->hbox = hbox;
	gtk_box_pack_start (GTK_BOX (eog_sidebar), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	select_button = gtk_menu_button_new ();
	gtk_widget_set_focus_on_click (select_button, TRUE);
	gtk_button_set_relief (GTK_BUTTON (select_button),
			       GTK_RELIEF_NONE);

	select_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	eog_sidebar->priv->label = gtk_label_new ("");
	gtk_widget_set_name (eog_sidebar->priv->label, "eog-sidebar-title");

	gtk_box_pack_start (GTK_BOX (select_hbox),
			    eog_sidebar->priv->label,
			    FALSE, FALSE, 0);

	gtk_widget_show (eog_sidebar->priv->label);

	arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_end (GTK_BOX (select_hbox), arrow, FALSE, FALSE, 0);
	eog_sidebar->priv->arrow = arrow;
	gtk_widget_set_visible (arrow, FALSE);

	gtk_container_add (GTK_CONTAINER (select_button), select_hbox);
	gtk_widget_show (select_hbox);

	gtk_box_set_center_widget (GTK_BOX (hbox), select_button);
	gtk_widget_show (select_button);

	close_button = gtk_button_new ();

	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);

	g_signal_connect (close_button, "clicked",
			  G_CALLBACK (eog_sidebar_close_clicked_cb),
			  eog_sidebar);
	gtk_widget_set_tooltip_text (close_button, _("Hide sidebar"));

	image = gtk_image_new_from_icon_name ("window-close-symbolic",
					      GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);

	gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);

	eog_sidebar->priv->list_box = gtk_list_box_new ();
	gtk_widget_show (eog_sidebar->priv->list_box);
	g_signal_connect (eog_sidebar->priv->list_box, "row-selected",
                          G_CALLBACK (eog_sidebar_menu_item_activate_cb),
                          eog_sidebar);

	eog_sidebar->priv->popover = gtk_popover_new (select_button);
	g_object_set (eog_sidebar->priv->list_box, "margin", 6, NULL);

	gtk_container_add (GTK_CONTAINER (eog_sidebar->priv->popover),
                           eog_sidebar->priv->list_box);

	gtk_menu_button_set_popover (GTK_MENU_BUTTON (select_button),
                                     eog_sidebar->priv->popover);

	eog_sidebar->priv->notebook = gtk_notebook_new ();

	gtk_notebook_set_show_border (GTK_NOTEBOOK (eog_sidebar->priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (eog_sidebar->priv->notebook), FALSE);

	gtk_box_pack_start (GTK_BOX (eog_sidebar), eog_sidebar->priv->notebook,
			    TRUE, TRUE, 0);

	gtk_widget_show (eog_sidebar->priv->notebook);
}

GtkWidget *
eog_sidebar_new (void)
{
	GtkWidget *eog_sidebar;

	eog_sidebar = g_object_new (EOG_TYPE_SIDEBAR,
				    "orientation", GTK_ORIENTATION_VERTICAL,
				    NULL);

	return eog_sidebar;
}

void
eog_sidebar_add_page (EogSidebar   *eog_sidebar,
		      const gchar  *title,
		      GtkWidget    *main_widget)
{
	GtkTreeIter iter;
	GtkWidget *label;
	GtkWidget *row;
	gchar *label_title;
	gint index;

	g_return_if_fail (EOG_IS_SIDEBAR (eog_sidebar));
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	index = gtk_notebook_append_page (GTK_NOTEBOOK (eog_sidebar->priv->notebook),
					  main_widget, NULL);

	label = gtk_label_new (title);
	gtk_widget_show (label);

	row = gtk_list_box_row_new ();
	gtk_widget_show (row);

	gtk_container_add (GTK_CONTAINER (row), label);
	gtk_container_add (GTK_CONTAINER (eog_sidebar->priv->list_box),
                           row);

	/* Insert and move to end */
	gtk_list_store_insert_with_values (GTK_LIST_STORE (eog_sidebar->priv->page_model),
					   &iter, 0,
					   PAGE_COLUMN_TITLE, title,
					   PAGE_COLUMN_MENU_ITEM, row,
					   PAGE_COLUMN_MAIN_WIDGET, main_widget,
					   PAGE_COLUMN_NOTEBOOK_INDEX, index,
					   -1);

	gtk_list_store_move_before (GTK_LIST_STORE(eog_sidebar->priv->page_model),
				    &iter,
				    NULL);

	/* Set the first item added as active */
	gtk_tree_model_get_iter_first (eog_sidebar->priv->page_model, &iter);
	gtk_tree_model_get (eog_sidebar->priv->page_model,
			    &iter,
			    PAGE_COLUMN_TITLE, &label_title,
			    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
			    -1);

	gtk_label_set_text (GTK_LABEL (eog_sidebar->priv->label), label_title);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (eog_sidebar->priv->notebook),
				       index);

	g_free (label_title);

	eog_sidebar_update_arrow_visibility (eog_sidebar);

	g_signal_emit (G_OBJECT (eog_sidebar),
		       signals[SIGNAL_PAGE_ADDED], 0, main_widget);
}

void
eog_sidebar_remove_page (EogSidebar *eog_sidebar, GtkWidget *main_widget)
{
	GtkTreeIter iter;
	GtkWidget *widget, *menu_item;
	gboolean valid;
	gint index;

	g_return_if_fail (EOG_IS_SIDEBAR (eog_sidebar));
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	valid = gtk_tree_model_get_iter_first (eog_sidebar->priv->page_model, &iter);

	while (valid) {
		gtk_tree_model_get (eog_sidebar->priv->page_model, &iter,
				    PAGE_COLUMN_NOTEBOOK_INDEX, &index,
				    PAGE_COLUMN_MENU_ITEM, &menu_item,
				    PAGE_COLUMN_MAIN_WIDGET, &widget,
				    -1);

		if (widget == main_widget) {
			break;
		} else {
			valid = gtk_tree_model_iter_next (eog_sidebar->priv->page_model,
							  &iter);
		}

		g_object_unref (menu_item);
		g_object_unref (widget);
	}

	if (valid) {
		gtk_notebook_remove_page (GTK_NOTEBOOK (eog_sidebar->priv->notebook),
					  index);

		gtk_container_remove (GTK_CONTAINER (eog_sidebar->priv->list_box), menu_item);

		gtk_list_store_remove (GTK_LIST_STORE (eog_sidebar->priv->page_model),
				       &iter);

		eog_sidebar_update_arrow_visibility (eog_sidebar);

		g_signal_emit (G_OBJECT (eog_sidebar),
			       signals[SIGNAL_PAGE_REMOVED], 0, main_widget);
	}
}

gint
eog_sidebar_get_n_pages (EogSidebar *eog_sidebar)
{
	g_return_val_if_fail (EOG_IS_SIDEBAR (eog_sidebar), TRUE);

	return gtk_tree_model_iter_n_children (
		GTK_TREE_MODEL (eog_sidebar->priv->page_model), NULL);
}

gboolean
eog_sidebar_is_empty (EogSidebar *eog_sidebar)
{
	g_return_val_if_fail (EOG_IS_SIDEBAR (eog_sidebar), TRUE);

	return gtk_tree_model_iter_n_children (
		GTK_TREE_MODEL (eog_sidebar->priv->page_model), NULL) == 0;
}
