/* GNOME libraries - abstract list selection model
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
#include "gnome-list-selection-model.h"



/* Signal IDs */
enum {
	IS_SELECTED,
	GET_MIN_SELECTED,
	GET_MAX_SELECTED,
	LAST_SIGNAL
};

static void gnome_list_selection_model_class_init (GnomeListSelectionModelClass *class);

static void marshal_is_selected (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);
static void marshal_get_minmax_selected (GtkObject *object, GtkSignalFunc func, gpointer data,
					 GtkArg *args);

static guint lsm_signals[LAST_SIGNAL];



/**
 * gnome_list_selection_model_get_type:
 * @void: 
 * 
 * Registers the &GnomeListSelectionModel class if necessary, and returns the
 * type ID associated to it.
 * 
 * Return value: The type ID of the &GnomeListSelectionModel class.
 **/
GtkType
gnome_list_selection_model_get_type (void)
{
	static GtkType lsm_type = 0;

	if (!lsm_type) {
		static const GtkTypeInfo lsm_info = {
			"GnomeListSelectionModel",
			sizeof (GnomeListSelectionModel),
			sizeof (GnomeListSelectionModelClass),
			(GtkClassInitFunc) gnome_list_selection_model_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		lsm_type = gtk_type_unique (gnome_list_model_get_type (), &lsm_info);
	}

	return lsm_type;
}

/* Class initialization function for the abstract list selection model */
static void
gnome_list_selection_model_class_init (GnomeListSelectionModelClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	lsm_signals[IS_SELECTED] =
		gtk_signal_new ("is_selected",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, is_selected),
				marshal_is_selected,
				GTK_TYPE_BOOL, 1,
				GTK_TYPE_UINT);
	lsm_signals[GET_MIN_SELECTED] =
		gtk_signal_new ("get_min_selected",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, get_min_selected),
				marshal_get_minmax_selected,
				GTK_TYPE_INT, 0);
	lsm_signals[GET_MAX_SELECTED] =
		gtk_signal_new ("get_max_selected",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, get_max_selected),
				marshal_get_minmax_selected,
				GTK_TYPE_INT, 0);

	gtk_object_class_add_signals (object_class, lsm_signals, LAST_SIGNAL);
}



/* Marshalers */

typedef gboolean (* IsSelectedFunc) (GtkObject *object, guint n, gpointer data);

static void
marshal_is_selected (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	IsSelectedFunc rfunc;
	gboolean *retval;

	retval = GTK_RETLOC_BOOL (args[1]);
	rfunc = (IsSelectedFunc) func;
	*retval = (* rfunc) (object, GTK_VALUE_UINT (args[0]), data);
}

typedef gint (* GetMinMaxSelectedFunc) (GtkObject *object, gpointer data);

static void
marshal_get_minmax_selected (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	GetMinMaxSelectedFunc rfunc;
	gint retval;

	retval = GTK_RETLOC_INT (args[0]);
	rfunc = (GetMinMaxSelectedFunc) func;
	*retval = (* rfunc) (object, data);
}



/* Exported functions */

/**
 * gnome_list_selection_model_is_selected:
 * @model: A list selection model.
 * @n: Index of item to query.
 * 
 * Queries whether a particular item in a list selection model is selected.
 * 
 * Return value: TRUE if the specified item is selected, FALSE otherwise.
 **/
gboolean
gnome_list_selection_model_is_selected (GnomeListSelectionModel *model, guint n)
{
	gboolean retval;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model), FALSE);
	g_return_val_if_fail (n < gnome_list_model_get_length (GNOME_LIST_MODEL (model)));

	retval = FALSE;
	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[IS_SELECTED], n, &retval);
	return retval;
}

/**
 * gnome_list_selection_model_get_min_selected:
 * @model: A list selection model.
 * 
 * Queries the first selected index of the items in a list selection model.
 * 
 * Return value: The first selected index or -1 if the selection is empty.
 **/
gint
gnome_list_selection_model_get_min_selected (GnomeListSelectionModel *model)
{
	gint retval;

	g_return_val_if_fail (model != NULL, -1);
	g_return_val_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model), -1);

	retval = -1;
	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[GET_MIN_SELECTED], &retval);
	return retval;
}

/**
 * gnome_list_selection_model_get_max_selected:
 * @model: A list selection model.
 * 
 * Queries the last selected index of the items in a list selection model.
 * 
 * Return value: The last selected index or -1 if the selection is empty.
 **/
gint
gnome_list_selection_model_get_max_selected (GnomeListSelectionModel *model)
{
	gint retval;

	g_return_val_if_fail (model != NULL, -1);
	g_return_val_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model), -1);

	retval = -1;
	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[GET_MAX_SELECTED], &retval);
	return retval;
}
