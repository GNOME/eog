/* GNOME libraries - Abstract icon list model
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
#include "gnome-icon-list-model.h"



#define CLASS(model) (GNOME_ICON_LIST_MODEL_CLASS (GTK_OBJECT (model)->klass))

/* Signal IDs */

enum {
	INTERVAL_CHANGED,
	INTERVAL_ADDED,
	INTERVAL_REMOVED,
	LAST_SIGNAL
};

static void ilm_class_init (GnomeIconListModelClass *class);

static void marshal_interval_notification (GtkObject *object, GtkSignalFunc func,
					   gpointer data, GtkArg *args);

static guint ilm_signals[LAST_SIGNAL];



/**
 * gnome_icon_list_model_get_type:
 *
 * Registers the #GnomeIconListModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeIconListModel class.
 **/
GtkType
gnome_icon_list_model_get_type (void)
{
	static GtkType ilm_type = 0;

	if (!ilm_type) {
		static const GtkTypeInfo ilm_info = {
			"GnomeIconListModel",
			sizeof (GnomeIconListModel),
			sizeof (GnomeIconListModelClass),
			(GtkClassInitFunc) ilm_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ilm_type = gtk_type_unique (GTK_TYPE_OBJECT, &ilm_info);
	}

	return ilm_type;
}

/* Class initialization function for the abstract icon list model */
static void
ilm_class_init (GnomeIconListModelClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	ilm_signals[INTERVAL_CHANGED] =
		gtk_signal_new ("interval_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeIconListModelClass, interval_changed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_INT);
	ilm_signals[INTERVAL_ADDED] =
		gtk_signal_new ("interval_added",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeIconListModelClass, interval_added),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_INT);

	ilm_signals[INTERVAL_REMOVED] =
		gtk_signal_new ("interval_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeIconListModelClass, interval_removed),
				marshal_interval_notification,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, ilm_signals, LAST_SIGNAL);
}



/* Signal marshalers */

typedef void (* IntervalNotificationFunc) (GtkObject *object, int start, int length, gpointer data);

static void
marshal_interval_notification (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	IntervalNotificationFunc rfunc;

	rfunc = (IntervalNotificationFunc) func;
	(* func) (object, GTK_VALUE_INT (args[0]), GTK_VALUE_INT (args[1]), data);
}



/**
 * gnome_icon_list_model_get_length:
 * @model: An icon list model.
 * 
 * Queries the number of items in an icon list model.
 * 
 * Return value: Number of items in the model.
 **/
int
gnome_icon_list_model_get_length (GnomeIconListModel *model)
{
	g_return_val_if_fail (model != NULL, -1);
	g_return_val_if_fail (GNOME_IS_ICON_LIST_MODEL (model), -1);

	g_assert (CLASS (model)->get_length != NULL);
	return (* CLASS (model)->get_length) (model);
}

/**
 * gnome_icon_list_model_get_item:
 * @model: An icon list model.
 * @n: Index of the sought item.
 * @pixbuf: Return value for the icon's image.
 * @caption: Return value for the icon's caption.
 * 
 * Queries a particular item in an icon list model.  The caller is reponsible
 * for doing gdk_pixbuf_unref() on the @pixbuf and for freeing the @caption.
 **/
void
gnome_icon_list_model_get_item (GnomeIconListModel *model, int n, GdkPixbuf **pixbuf, char **caption)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));

	g_assert (CLASS (model)->get_item != NULL);
	return (* CLASS (model)->get_item) (model, n, pixbuf, caption);
}

/**
 * gnome_icon_list_model_interval_changed:
 * @model: An icon list model.
 * @start: Index of the first item that changed.
 * @length: Number of items that changed.
 * 
 * Causes an icon list model to emit the "interval_changed" signal.  This
 * function should be used by icon list model implementations to notify
 * interested parties of changes to existing items in the model.
 **/
void
gnome_icon_list_model_interval_changed (GnomeIconListModel *model, int start, int length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));
	g_return_if_fail (start >= 0 && length > 0);

	gtk_signal_emit (GTK_OBJECT (model), ilm_signals[INTERVAL_CHANGED], start, length);
}

/**
 * gnome_icon_list_model_interval_added:
 * @model: An icon list model.
 * @start: Index of the first item that was added.
 * @length: Number of items that were added.
 * 
 * Causes an icon list model to emit the "interval_added" signal.  This function
 * should be used by icon list model implementations to notify interested
 * parties when a reange of items is inserted in the model.
 **/
void
gnome_icon_list_model_interval_added (GnomeIconListModel *model, int start, int length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));
	g_return_if_fail (start >= 0 && length > 0);

	gtk_signal_emit (GTK_OBJECT (model), ilm_signals[INTERVAL_ADDED], start, length);
}

/**
 * gnome_icon_list_model_interval_removed:
 * @model: An icon list model.
 * @start: Index of the first item that was removed.
 * @length: Number of items that were removed.
 * 
 * Causes an icon list model to emit the "interval_removed" signal.  This
 * function should be used by icon list model implementations to notify
 * interested parties when a range of items is removed from the model.
 **/
void
gnome_icon_list_model_interval_removed (GnomeIconListModel *model, int start, int length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));
	g_return_if_fail (start >= 0 && length > 0);

	gtk_signal_emit (GTK_OBJECT (model), ilm_signals[INTERVAL_REMOVED], start, length);
}
