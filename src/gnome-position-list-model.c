/* GNOME libraries - abstract position list model
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
#include <gtk/gtksignal.h>
#include "gnome-position-list-model.h"



/* Signal IDs */
enum {
	GET_POSITION,
	LAST_SIGNAL
};

static void gnome_position_list_model_class_init (GnomePositionListModelClass *class);

static void marshal_get_position (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);

static guint plm_signals[LAST_SIGNAL];



/**
 * gnome_position_list_model_get_type:
 * @void:
 *
 * Registers the &GnomePositionListModel class if necessary, and returns the
 * type ID associated to it.
 *
 * Return value: The type ID of the &GnomePositionListModel class.
 **/
GtkType
gnome_position_list_model_get_type (void)
{
	static GtkType plm_type = 0;

	if (!plm_type) {
		static const GtkTypeInfo plm_info = {
			"GnomePositionListModel",
			sizeof (GnomePositionListModel),
			sizeof (GnomePositionListModelClass),
			(GtkClassInitFunc) gnome_position_list_model_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		plm_type = gtk_type_unique (gnome_list_model_get_type (), &plm_info);
	}

	return plm_type;
}

/* Class initialization function for the abstract position list model */
static void
gnome_position_list_model_class_init (GnomePositionListModelClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	plm_signals[GET_POSITION] =
		gtk_signal_new ("get_position",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomePositionListModelClass, get_position),
				marshal_get_position,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_UINT,
				GTK_TYPE_POINTER,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, plm_signals, LAST_SIGNAL);
}



/* Marshalers */

typedef void (* GetPositionFunc) (GtkObject *object, guint n, gpointer x, gpointer y, gpointer data);

static void
marshal_get_position (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	GetPositionFunc rfunc;

	rfunc = (GetPositionFunc) func;
	(* rfunc) (object, GTK_VALUE_UINT (args[0]), GTK_VALUE_POINTER (args[1]),
		   GTK_VALUE_POINTER (args[2]), data);
}



/* Exported functions */

/**
 * gnome_position_list_model_get_position:
 * @model: A position list model.
 * @n: Index of item to query.
 * @x: Return value for the X coordinate.
 * @y: Return value for the Y coordinate.
 *
 * Queries the coordinate data for a position in a position list model.
 **/
void
gnome_position_list_model_get_position (GnomePositionListModel *model, guint n,
					gint *x, gint *y)
{
	gint rx, ry;

	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_POSITION_LIST_MODEL (model));

	rx = ry = 0;

	gtk_signal_emit (GTK_OBJECT (model), plm_signals[GET_POSITION], n, &rx, &ry);

	if (x)
		*x = rx;

	if (y)
		*y = ry;
}
