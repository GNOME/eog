/* GNOME libraries - abstract list model
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
#include "gnome-list-model.h"



/* Signal IDs */
enum {
	INTERVAL_CHANGED,
	INTERVAL_ADDED,
	INTERVAL_REMOVED,
	LAST_SIGNAL
};

static void gnome_list_model_class_init (GnomeListModelClass *class);

static guint list_model_signals[LAST_SIGNAL];



/**
 * gnome_list_model_get_type:
 * @void:
 *
 * Registers the &GnomeListModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GnomeListModel class.
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
