/* GNOME libraries - abstract list view
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtksignal.h>
#include "gnome-list-view.h"



/* Private part of the GnomeListView structure */
typedef struct {
	/* The model we are displaying */
	GnomeListModel *model;

	/* Selection model we are using */
	GnomeListSelectionModel *sel_model;

	/* The list item factory */
	GnomeListItemFactory *factory;
} ListViewPrivate;



/* Signal IDs */
enum {
	MODEL_SET,
	SELECTION_MODEL_SET,
	LIST_ITEM_FACTORY_SET,
	LAST_SIGNAL
};

static void gnome_list_view_class_init (GnomeListViewClass *class);
static void gnome_list_view_init (GnomeListView *view);
static void gnome_list_view_destroy (GtkObject *object);

static GtkContainerClass *parent_class;

static guint list_view_signals[LAST_SIGNAL];



/**
 * gnome_list_view_get_type:
 * @void:
 *
 * Registers the #GnomeListView class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeListView class.
 **/
GtkType
gnome_list_view_get_type (void)
{
	static GtkType list_view_type = 0;

	if (!list_view_type) {
		static const GtkTypeInfo list_view_info = {
			"GnomeListView",
			sizeof (GnomeListView),
			sizeof (GnomeListViewClass),
			(GtkClassInitFunc) gnome_list_view_class_init,
			(GtkObjectInitFunc) gnome_list_view_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		list_view_type = gtk_type_unique (gtk_container_get_type (), &list_view_info);
	}

	return list_view_type;
}

/* Class initialization function for the abstract list view */
static void
gnome_list_view_class_init (GnomeListViewClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gtk_container_get_type ());

	list_view_signals[MODEL_SET] =
		gtk_signal_new ("model_set",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, model_set),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_MODEL);
	list_view_signals[SELECTION_MODEL_SET] =
		gtk_signal_new ("selection_model_set",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, selection_model_set),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_SELECTION_MODEL);
	list_view_signals[LIST_ITEM_FACTORY_SET] =
		gtk_signal_new ("list_item_factory_set",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, list_item_factory_set),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_ITEM_FACTORY);

	gtk_object_class_add_signals (object_class, list_view_signals, LAST_SIGNAL);

	object_class->destroy = gnome_list_view_destroy;
}

/* Object initialization function for the abstract list view */
static void
gnome_list_view_init (GnomeListView *view)
{
	ListViewPrivate *priv;

	priv = g_new0 (ListViewPrivate, 1);
	view->priv = priv;
}

/* Destroy handler for the abstract list view */
static void
gnome_list_view_destroy (GtkObject *object)
{
	GnomeListView *view;
	ListViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (object));

	view = GNOME_LIST_VIEW (object);
	priv = view->priv;

	if (priv->model)
		gtk_object_unref (GTK_OBJECT (priv->model));

	if (priv->sel_model)
		gtk_object_unref (GTK_OBJECT (priv->sel_model));

	if (priv->factory)
		gtk_object_unref (GTK_OBJECT (priv->factory));

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Exported functions */

/**
 * gnome_list_view_set_model:
 * @view: A list view.
 * @model: List model to display.
 *
 * Sets the list model that a list view will display.  This function is for use
 * by derived classes of #GnomeListView only; they can get notification about
 * the change of data model with the "model_set" signal.
 **/
void
gnome_list_view_set_model (GnomeListView *view, GnomeListModel *model)
{
	ListViewPrivate *priv;
	GnomeListModel *old_model;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));
	if (model)
		g_return_if_fail (GNOME_IS_LIST_MODEL (model));

	priv = view->priv;

	if (model == priv->model)
		return;

	if (model)
		gtk_object_ref (GTK_OBJECT (model));

	old_model = priv->model;
	priv->model = model;

	gtk_signal_emit (GTK_OBJECT (view), list_view_signals[MODEL_SET], old_model);

	if (old_model)
		gtk_object_unref (GTK_OBJECT (old_model));
}

/**
 * gnome_list_view_get_model:
 * @view: A list view.
 *
 * Queries the list model that a list view is displaying.
 *
 * Return value: The list model.
 **/
GnomeListModel *
gnome_list_view_get_model (GnomeListView *view)
{
	ListViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_LIST_VIEW (view), NULL);

	priv = view->priv;

	return priv->model;
}

/**
 * gnome_list_view_set_selection_model:
 * @view: A list view.
 * @sel_model: List selection model to use.
 *
 * Sets the list selection model for a list view.
 **/
void
gnome_list_view_set_selection_model (GnomeListView *view, GnomeListSelectionModel *sel_model)
{
	ListViewPrivate *priv;
	GnomeListSelectionModel *old_sel_model;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));
	if (sel_model)
		g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (sel_model));

	priv = view->priv;

	if (sel_model == priv->sel_model)
		return;

	if (sel_model)
		gtk_object_ref (GTK_OBJECT (sel_model));

	old_sel_model = priv->sel_model;
	priv->sel_model = sel_model;

	gtk_signal_emit (GTK_OBJECT (view), list_view_signals[SELECTION_MODEL_SET], old_sel_model);

	if (old_sel_model)
		gtk_object_unref (GTK_OBJECT (old_sel_model));
}

/**
 * gnome_list_view_get_selection_model:
 * @view: A list view.
 *
 * Queries the list selection model that a list view is using.
 *
 * Return value: The list selection model.
 **/
GnomeListSelectionModel *
gnome_list_view_get_selection_model (GnomeListView *view)
{
	ListViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_LIST_VIEW (view), NULL);

	priv = view->priv;
	return priv->sel_model;
}

/**
 * gnome_list_view_set_list_item_factory:
 * @view: A list view.
 * @factory: List item factory to use.
 *
 * Sets the list item factory that a list view will use to render the list
 * items.  This function is for use by derived classes of #GnomeListView only;
 * they can get notification about the change of item factory with the
 * "list_item_factory_set" signal.
 **/
void
gnome_list_view_set_list_item_factory (GnomeListView *view, GnomeListItemFactory *factory)
{
	ListViewPrivate *priv;
	GnomeListItemFactory *old_factory;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));
	if (factory)
		g_return_if_fail (GNOME_IS_LIST_ITEM_FACTORY (factory));

	priv = view->priv;

	if (factory == priv->factory)
		return;

	if (factory)
		gtk_object_ref (GTK_OBJECT (factory));

	old_factory = priv->factory;
	priv->factory = factory;

	gtk_signal_emit (GTK_OBJECT (view), list_view_signals[LIST_ITEM_FACTORY_SET], old_factory);

	if (old_factory)
		gtk_object_unref (GTK_OBJECT (old_factory));
}

/**
 * gnome_list_view_get_list_item_factory:
 * @view: A list view.
 *
 * Queries the list item factory that a list view is using.
 *
 * Return value: The list item factory.
 **/
GnomeListItemFactory *
gnome_list_view_get_list_item_factory (GnomeListView *view)
{
	ListViewPrivate *priv;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_LIST_VIEW (view), NULL);

	priv = view->priv;

	return priv->factory;
}
