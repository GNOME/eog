/* GNOME libraries - abstract list model
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
#include <gtk/gtksignal.h>
#include "gnome-list-model.h"



/* Signal IDs */
enum {
	GET_LENGTH,
	INTERVAL_CHANGED,
	INTERVAL_ADDED,
	INTERVAL_REMOVED,
	LAST_SIGNAL
};

static void gnome_list_model_class_init (GnomeListModelClass *class);

static void marshal_get_length (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);
static void marshal_interval_notification (GtkObject *object, GtkSignalFunc func,
					   gpointer data, GtkArg *args);

static guint list_model_signals[LAST_SIGNAL];



/**
 * gnome_list_model_get_type:
 * @void:
 *
 * Registers the #GnomeListModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeListModel class.
 **/
GtkType
gnome_list_model_get_type (void)
{
	static GtkType list_model_type = 0;

	if (!list_model_type) {
		static const GtkTypeInfo list_model_info = {
			"GnomeListModel",
			sizeof (GnomeListModel),
			sizeof (GnomeListModelClass),
			(GtkClassInitFunc) gnome_list_model_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		list_model_type = gtk_type_unique (gtk_object_get_type (), &list_model_info);
	}

	return list_model_type;
}

/* Class initialization function for the abstract list model */
static void
gnome_list_model_class_init (GnomeListModelClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	list_model_signals[GET_LENGTH] =
		gtk_signal_new ("get_length",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListModelClass, get_length),
				marshal_get_length,
				GTK_TYPE_UINT, 0);
	list_model_signals[INTERVAL_CHANGED] =
		gtk_signal_new ("interval_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListModelClass, interval_changed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	list_model_signals[INTERVAL_ADDED] =
		gtk_signal_new ("interval_added",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListModelClass, interval_added),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	list_model_signals[INTERVAL_REMOVED] =
		gtk_signal_new ("interval_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListModelClass, interval_removed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);

	gtk_object_class_add_signals (object_class, list_model_signals, LAST_SIGNAL);
}



/* Marshalers */

typedef guint (* GetLengthFunc) (GtkObject *object, gpointer data);

static void
marshal_get_length (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	GetLengthFunc rfunc;
	guint *retval;

	retval = GTK_RETLOC_UINT (args[0]);
	rfunc = (GetLengthFunc) func;
	*retval = (* rfunc) (object, data);
}

typedef void (* IntervalNotificationFunc) (GtkObject *object, guint start, guint length,
					   gpointer data);

static void
marshal_interval_notification (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	IntervalNotificationFunc rfunc;

	rfunc = (IntervalNotificationFunc) func;
	(* func) (object, GTK_VALUE_UINT (args[0]), GTK_VALUE_UINT (args[1]), data);
}



/* Exported functions */

/**
 * gnome_list_model_get_length:
 * @model: A list model.
 *
 * Queries the length of the list in a list model.
 *
 * Return value: the length of the list.
 **/
guint
gnome_list_model_get_length (GnomeListModel *model)
{
	guint retval;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (GNOME_IS_LIST_MODEL (model), 0);

	retval = 0;
	gtk_signal_emit (GTK_OBJECT (model), list_model_signals[GET_LENGTH], &retval);
	return retval;
}

/**
 * gnome_list_model_interval_changed:
 * @model: A list model.
 * @start: Index of first item that changed.
 * @length: Number of items that changed.
 *
 * Causes a list model to emit the "interval_changed" signal.  This function can
 * be used by list model implementations to notify interested parties of changes
 * to existing items in the model.
 **/
void
gnome_list_model_interval_changed (GnomeListModel *model, guint start, guint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), list_model_signals[INTERVAL_CHANGED], start, length);
}

/**
 * gnome_list_model_interval_added:
 * @model: A list model.
 * @start: Index of first item that was added.
 * @length: Number of items that were added.
 *
 * Causes a list model to emit the "interval_added" signal.  This function can
 * be used by list model implementations to notify interested parties when a
 * range of items is inserted in the model.
 **/
void
gnome_list_model_interval_added (GnomeListModel *model, guint start, guint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), list_model_signals[INTERVAL_ADDED], start, length);
}

/**
 * gnome_list_model_interval_removed:
 * @model: A list model.
 * @start: Index of first item that was removed.
 * @length: Number of items that were removed.
 *
 * Causes a list model to emit the "interval_removed" signal.  This function can
 * be used by list model implementations to notify interested parties when a
 * range of items is removed from the model.
 **/
void
gnome_list_model_interval_removed (GnomeListModel *model, guint start, guint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), list_model_signals[INTERVAL_REMOVED], start, length);
}
