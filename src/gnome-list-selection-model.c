/* GNOME libraries - abstract list selection model
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
#include "gnome-list-selection-model.h"



/* Private part of the GnomeListSelectionModel structure */
typedef struct {
	/* Whether multiple changes are being done to the selection state */
	guint is_adjusting : 1;
} LSMPrivate;



/* Signal IDs */
enum {
	CLEAR,
	SET_INTERVAL,
	ADD_INTERVAL,
	REMOVE_INTERVAL,
	IS_SELECTED,
	GET_MIN_SELECTED,
	GET_MAX_SELECTED,
	LAST_SIGNAL
};

static void gnome_list_selection_model_class_init (GnomeListSelectionModelClass *class);
static void gnome_list_selection_model_init (GnomeListSelectionModel *model);
static void gnome_list_selection_model_destroy (GtkObject *object);

static void marshal_interval (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);
static void marshal_is_selected (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);
static void marshal_get_minmax_selected (GtkObject *object, GtkSignalFunc func, gpointer data,
					 GtkArg *args);

static GnomeListModelClass *parent_class;

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
			(GtkObjectInitFunc) gnome_list_selection_model_init,
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

	parent_class = gtk_type_class (gnome_list_model_get_type ());

	lsm_signals[CLEAR] =
		gtk_signal_new ("clear",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, clear),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	lsm_signals[SET_INTERVAL] =
		gtk_signal_new ("set_interval",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, set_interval),
				marshal_interval,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	lsm_signals[ADD_INTERVAL] =
		gtk_signal_new ("add_interval",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, add_interval),
				marshal_interval,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
	lsm_signals[REMOVE_INTERVAL] =
		gtk_signal_new ("remove_interval",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListSelectionModelClass, remove_interval),
				marshal_interval,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_UINT,
				GTK_TYPE_UINT);
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

	object_class->destroy = gnome_list_selection_model_destroy;
}

/* Object initialization function for the abstract list selection model */
static void
gnome_list_selection_model_init (GnomeListSelectionModel *model)
{
	LSMPrivate *priv;

	priv = g_new0 (LSMPrivate, 1);
	model->priv = priv;
}

/* Destroy handler for the abstract list selection model */
static void
gnome_list_selection_model_destroy (GtkObject *object)
{
	GnomeListSelectionModel *model;
	LSMPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (object));

	model = GNOME_LIST_SELECTION_MODEL (object);
	priv = model->priv;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Marshalers */

typedef void (* IntervalFunc) (GtkObject *object, guint start, guint length, gpointer data);

static void
marshal_interval (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	IntervalFunc rfunc;

	rfunc = (IntervalFunc) func;
	(* rfunc) (object, GTK_VALUE_UINT (args[0]), GTK_VALUE_UINT (args[1]), data);
}

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
	gint *retval;

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
	g_return_val_if_fail (n < gnome_list_model_get_length (GNOME_LIST_MODEL (model)), FALSE);

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

/**
 * gnome_list_selection_model_set_is_adjusting:
 * @model: A list selection model.
 * @is_adjusting: New value of the is_adjusting property.
 *
 * A list selection model can be said to be adjusting if multiple changes are
 * being made to the selection status, for example, if the user is rubberbanding
 * with the mouse and items are being changed continuously.  Clients can use
 * this flag to determine when to actually update their state based on the
 * selection value; if it is TRUE, they may prefer to wait until the selection
 * status obtains its final value.
 **/
void
gnome_list_selection_model_set_is_adjusting (GnomeListSelectionModel *model, gboolean is_adjusting)
{
	LSMPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model));

	priv = model->priv;
	priv->is_adjusting = is_adjusting ? TRUE : FALSE;
}

/**
 * gnome_list_selection_model_get_is_adjusting:
 * @model: A list selection model.
 *
 * Queries whether a list selection model's status is being changed
 * continuously.
 *
 * Return value: TRUE if clients can ignore changes to the selection status,
 * since they may be transitory, or FALSE if they should consider the selection
 * status to be final.
 **/
gboolean
gnome_list_selection_model_get_is_adjusting (GnomeListSelectionModel *model)
{
	LSMPrivate *priv;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model), FALSE);

	priv = model->priv;
	return priv->is_adjusting;
}

/**
 * gnome_list_selection_model_clear:
 * @model: A list selection model.
 *
 * Tells a list selection model to unselect all the items.
 **/
void
gnome_list_selection_model_clear (GnomeListSelectionModel *model)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[CLEAR]);
}

/**
 * gnome_list_selection_model_set_interval:
 * @model: A list selection model.
 * @start: First item to select.
 * @length: Number of items to select.
 *
 * Changes the selection of a list selection model to the specified interval.
 **/
void
gnome_list_selection_model_set_interval (GnomeListSelectionModel *model, guint start, guint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[SET_INTERVAL], start, length);
}

/**
 * gnome_list_selection_model_add_interval:
 * @model: A list selection model.
 * @start: First item to add to the selection.
 * @length: Number of items to add to the selection.
 *
 * Adds the specified interval to the selection in a list selection model.
 **/
void
gnome_list_selection_model_add_interval (GnomeListSelectionModel *model, guint start, guint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[ADD_INTERVAL], start, length);
}

/**
 * gnome_list_selection_model_remove_interval:
 * @model: A list selection model.
 * @start: First item to remove from the selection.
 * @length: Number of items to remove from the selection.
 *
 * Removes the specified interval from the selection in a list selection model.
 **/
void
gnome_list_selection_model_remove_interval (GnomeListSelectionModel *model, guint start, guint length)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_SELECTION_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (model), lsm_signals[REMOVE_INTERVAL], start, length);
}
