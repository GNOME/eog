/* GNOME libraries - abstract list view
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
	SET_MODEL,
	SET_SELECTION_MODEL,
	SET_LIST_ITEM_FACTORY,
	LAST_SIGNAL
};

static void gnome_list_view_class_init (GnomeListViewClass *class);
static void gnome_list_view_init (GnomeListView *view);
static void gnome_list_view_destroy (GtkObject *object);

static void set_model (GnomeListView *view, GnomeListModel *model);
static void set_selection_model (GnomeListView *view, GnomeListSelectionModel *sel_model);
static void set_list_item_factory (GnomeListView *view, GnomeListItemFactory *factory);

static GtkContainerClass *parent_class;

static guint list_view_signals[LAST_SIGNAL];



/**
 * gnome_list_view_get_type:
 * @void:
 *
 * Registers the &GnomeListView class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GnomeListView class.
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

	list_view_signals[SET_MODEL] =
		gtk_signal_new ("set_model",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, set_model),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_MODEL);
	list_view_signals[SET_SELECTION_MODEL] =
		gtk_signal_new ("set_selection_model",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, set_selection_model),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_SELECTION_MODEL);
	list_view_signals[SET_LIST_ITEM_FACTORY] =
		gtk_signal_new ("set_list_item_factory",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, set_list_item_factory),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_ITEM_FACTORY);

	gtk_object_class_add_signals (object_class, list_view_signals, LAST_SIGNAL);

	object_class->destroy = gnome_list_view_destroy;

	class->set_model = set_model;
	class->set_selection_model = set_selection_model;
	class->set_list_item_factory = set_list_item_factory;
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



/* Default signal handlers */

/* Set_model handler for the abstract list view */
static void
set_model (GnomeListView *view, GnomeListModel *model)
{
	ListViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));

	if (model)
		g_return_if_fail (GNOME_IS_LIST_MODEL (model));

	priv = view->priv;

	if (model == priv->model)
		return;

	if (model)
		gtk_object_ref (GTK_OBJECT (model));

	if (priv->model)
		gtk_object_unref (GTK_OBJECT (priv->model));

	priv->model = model;
	/* FIXME: update view */
}

/* Set_selection_model handler for the abstract list view */
static void
set_selection_model (GnomeListView *view, GnomeListSelectionModel *sel_model)
{
	ListViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));

	if (sel_model)
		g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (sel_model));

	priv = view->priv;

	if (sel_model == priv->sel_model)
		return;

	if (sel_model)
		gtk_object_ref (GTK_OBJECT (sel_model));

	if (priv->sel_model)
		gtk_object_unref (GTK_OBJECT (priv->sel_model));

	priv->sel_model = sel_model;
	/* FIXME: update view */
}

/* Set_list_item_factory handler for the abstract list view */
static void
set_list_item_factory (GnomeListView *view, GnomeListItemFactory *factory)
{
	ListViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));

	if (factory)
		g_return_if_fail (GNOME_IS_LIST_ITEM_FACTORY (factory));

	priv = view->priv;

	if (factory == priv->factory)
		return;

	if (factory)
		gtk_object_ref (GTK_OBJECT (factory));

	if (priv->factory)
		gtk_object_unref (GTK_OBJECT (priv->factory));

	priv->factory = factory;
	/* FIXME: update view */
}



/* Exported functions */

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
 * @sel_model: A list selection model.
 * 
 * Sets the list selection model for a list view.
 **/
void
gnome_list_view_set_selection_model (GnomeListView *view, GnomeListSelectionModel *sel_model)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_LIST_VIEW (view));

	if (sel_model)
		g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (sel_model));

	gtk_signal_emit (GTK_OBJECT (view), list_view_signals[SET_SELECTION_MODEL],
			 sel_model);
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
