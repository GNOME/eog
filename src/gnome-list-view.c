/* GNOME libraries - abstract list view
 *
 * Copyright (C) 1999 The Free Software Foundation
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
#include <gtk/gtkmarshal.h>
#include "gnome-list-view.h"



/* Private part of the GnomeListView structure */
typedef struct {
	/* The model we are displaying */
	GnomeListModel *model;

	/* The item view factory */
	GnomeItemViewFactory *factory;
} ListViewPrivate;



/* Signal IDs */
enum {
	SET_MODEL,
	SET_ITEM_VIEW_FACTORY,
	LAST_SIGNAL
};

static void gnome_list_view_class_init (GnomeListViewClass *class);
static void gnome_list_view_init (GnomeListView *view);
static void gnome_list_view_destroy (GtkObject *object);

static void set_model (GnomeListView *view, GnomeListModel *model);
static void set_item_view_factory (GnomeListView *view, GnomeItemViewFactory *factory);

static GnomeCanvasClass *parent_class;

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

		list_view_type = gtk_type_unique (gnome_canvas_get_type (), &list_view_info);
	}

	return list_view_type;
}

/* Class initialization function for the abstract list view */
static void
gnome_list_view_class_init (GnomeListViewClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

	list_view_signals[SET_MODEL] =
		gtk_signal_new ("set_model",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, set_model),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_LIST_MODEL);
	list_view_signals[SET_ITEM_VIEW_FACTORY] =
		gtk_signal_new ("set_item_view_factory",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListViewClass, set_item_view_factory),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GNOME_TYPE_ITEM_VIEW_FACTORY);

	gtk_object_class_add_signals (object_class, list_view_signals, LAST_SIGNAL);

	class->set_model = set_model;
	class->set_item_view_factory = set_item_view_factory;
}



/* Default signal handlers */

/* Set_model handler for the abstract list view */
static void
set_model (GnomeListView *view, GnomeListModel *model)
{
	ListViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_GNOME_LIST_VIEW (view));

	if (model)
		g_return_if_fail (GNOME_IS_LIST_MODEL (model));

	priv = view->priv;

	
}
