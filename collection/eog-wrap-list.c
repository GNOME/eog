/* EogWrapList - view of the image collection
 *
 * Copyright (C) 2001 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include "eog-wrap-list.h"



/* Used to hold signal handler IDs for models */
typedef struct {
	guint interval_changed_id;
	guint interval_added_id;
	guint interval_removed_id;
} ModelSignalIDs;

enum {
	ITEM_DBL_CLICKED,
	LAST_SIGNAL
};

static guint eog_wrap_list_signals [LAST_SIGNAL];

enum {
	GLOBAL_SIZE_CHANGED,
	GLOBAL_SPACING_CHANGED,
	GLOBAL_FACTORY_CHANGED,
	GLOBAL_MODEL_CHANGED,
	GLOBAL_HINT_LAST
};

typedef enum {
	ITEM_CHANGED,
	ITEM_ADDED,
	ITEM_REMOVED,
	ITEM_HINT_LAST
} ItemHint;

typedef struct {
	ItemHint hint;
	GList *id_list;
} ItemUpdate;

struct _EogWrapListPrivate {
	/* List of all items currently in the view. */
	GHashTable *item_table;
	
	/* Factory to use. */
	GnomeListItemFactory *factory;

	/* Model to use. */
	EogCollectionModel *model;

	/* Signal connection IDs for the models */
	ModelSignalIDs model_ids;
	
	/* Group which hold all the items. */
	GnomeCanvasItem *item_group;

	/* spacing between items */
	guint row_spacing;
	guint col_spacing;

	/* size of items */
	guint item_width;
	guint item_height;

	/* Number of items */
	guint n_items; 

	/* Number of rows. */
	guint n_rows;

	/* Number of columns. */
	guint n_cols;

	/* Id for the idle handler.*/
	gint idle_handler_id;

	/* Update hints */
	gboolean global_update_hints [GLOBAL_HINT_LAST];

	gboolean is_updating;

	/* unique IDs to update */
	GList *item_update_list;
};

static void eog_wrap_list_class_init (EogWrapListClass *class);
static void eog_wrap_list_init (EogWrapList *wlist);
static void eog_wrap_list_destroy (GtkObject *object);
static void eog_wrap_list_finalize (GtkObject *object);

static void eog_wrap_list_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void eog_wrap_list_clear (EogWrapList *wlist);

static void request_update (EogWrapList *wlist);
static void update (EogWrapList *wlist);

static GnomeCanvasClass *parent_class;
#define PARENT_TYPE gnome_canvas_get_type ()



/**
 * eog_wrap_list_get_type:
 * @void:
 *
 * Registers the #EogWrapList class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #EogWrapList class.
 **/
GtkType
eog_wrap_list_get_type (void)
{
	static GtkType wrap_list_type = 0;

	if (!wrap_list_type) {
		static const GtkTypeInfo wrap_list_info = {
			"EogWrapList",
			sizeof (EogWrapList),
			sizeof (EogWrapListClass),
			(GtkClassInitFunc) eog_wrap_list_class_init,
			(GtkObjectInitFunc) eog_wrap_list_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		wrap_list_type = gtk_type_unique (PARENT_TYPE, &wrap_list_info);
	}

	return wrap_list_type;
}

/* Class initialization function for the abstract wrapped list view */
static void
eog_wrap_list_class_init (EogWrapListClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = eog_wrap_list_destroy;
	object_class->finalize = eog_wrap_list_finalize;

	widget_class->size_allocate = eog_wrap_list_size_allocate;

	eog_wrap_list_signals [ITEM_DBL_CLICKED] = 
		gtk_signal_new ("item_dbl_click",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogWrapListClass, item_dbl_click),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);
	gtk_object_class_add_signals (object_class, eog_wrap_list_signals, LAST_SIGNAL);
}

/* object initialization function for the abstract wrapped list view */
static void
eog_wrap_list_init (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;

	priv = g_new0 (EogWrapListPrivate, 1);
	wlist->priv = priv;
	
	priv->item_table = NULL;
	priv->model = NULL;
	priv->factory = NULL;
	priv->row_spacing = 0;
	priv->col_spacing = 0;
	priv->idle_handler_id = -1;
	priv->is_updating = FALSE;
	priv->n_items = 0;
	priv->n_rows = 0;
	priv->n_cols = 0;
}

/* Destroy handler for the abstract wrapped list view */
static void
eog_wrap_list_destroy (GtkObject *object)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (object));

	wlist = EOG_WRAP_LIST (object);
	priv = wlist->priv;

	if (priv->model)
		gtk_object_unref (GTK_OBJECT (priv->model));
	priv->model = NULL;

	if (priv->factory)
		gtk_object_unref (GTK_OBJECT (priv->factory));
	priv->factory = NULL;

	/* FIXME: free the items and item array */

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
eog_wrap_list_finalize (GtkObject *object)
{
	EogWrapList *wlist;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (object));

	wlist = EOG_WRAP_LIST (object);
	if (wlist->priv)
		g_free (wlist->priv);
	wlist->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
eog_wrap_list_construct (EogWrapList *wlist)
{
	wlist->priv->item_table = g_hash_table_new (NULL, NULL);
}


GtkWidget*
eog_wrap_list_new (void)
{
	GtkWidget *wlist;

        gtk_widget_push_visual (gdk_rgb_get_visual ());
        gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	wlist = gtk_widget_new (eog_wrap_list_get_type (), NULL);

        gtk_widget_pop_visual ();
        gtk_widget_pop_colormap ();

	eog_wrap_list_construct (EOG_WRAP_LIST (wlist));

	return wlist;
}


static gint
handle_item_event (GnomeCanvasItem *item, GdkEvent *event,  gpointer data) 
{
	EogWrapList *wlist;
	gint n;

	wlist = EOG_WRAP_LIST (data);

#if 0
	smodel = gnome_list_view_get_selection_model (GNOME_LIST_VIEW (view));
	if (smodel == NULL) return FALSE;
#endif

	n = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "IconNumber"));

	switch (event->type) {
	case GDK_BUTTON_PRESS:
#if 0
		if ((event->button.state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) {
			/* shift button pressed */
			if (previous_n == -1) {
				gnome_list_selection_model_set_interval (smodel, n, 1);
			} else {
				gint max, min;
				
				max = MAX (previous_n, n);
				min = MIN (previous_n, n);
				
				gnome_list_selection_model_add_interval (smodel, min, max-min+1);
			}
		} else if ((event->button.state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK) {
			/* ctrl button pressed */ 
			if (gnome_list_selection_model_is_selected (smodel, n))
				gnome_list_selection_model_remove_interval (smodel, n, 1);
			else
				gnome_list_selection_model_add_interval (smodel, n, 1);
		} else
			gnome_list_selection_model_set_interval (smodel, n, 1);

		previous_n = n;
#endif
		break;
		
	case GDK_2BUTTON_PRESS:
		g_print ("Double click on item: %i\n", n);
		gtk_signal_emit (GTK_OBJECT (wlist), 
				 eog_wrap_list_signals [ITEM_DBL_CLICKED],
				 n);
		break;
		
	default:
		return FALSE;
	}

	return TRUE;
}

/* Size_request handler for the abstract wrapped list view */
static void
eog_wrap_list_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	int border_width;

	wlist = EOG_WRAP_LIST (widget);
	priv = wlist->priv;

	gtk_widget_size_request (GTK_WIDGET (wlist), requisition);

	border_width = GTK_CONTAINER (widget)->border_width;

	requisition->width += 2 * border_width;
	requisition->height += 2 * border_width;
}

/* Size_allocate handler for the abstract wrapped list view */
static void
eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	wlist = EOG_WRAP_LIST (widget);
	priv = wlist->priv;

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

	priv->global_update_hints[GLOBAL_SIZE_CHANGED] = TRUE;

	request_update (wlist);
}       

/* Notifications from models */

/* Handler for the interval_changed signal from models */
static void
model_interval_changed (EogCollectionModel *model, GList *id_list, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	ItemUpdate *update;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	g_message ("model_interval_changed called\n");

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;

	if (priv->global_update_hints[GLOBAL_FACTORY_CHANGED] ||
	    priv->global_update_hints[GLOBAL_MODEL_CHANGED]   ||
	    priv->global_update_hints[GLOBAL_SPACING_CHANGED])
	{
		/* if any global changes have to be done already, we don't need
		   to add any specific item changes */
		return;
	}
	
	update = g_new0 (ItemUpdate, 1);
	update->hint = ITEM_CHANGED;
	update->id_list = id_list;
	
	priv->item_update_list = g_list_append (priv->item_update_list, update);

	request_update (wlist);
}

/* Handler for the interval_added signal from models */
static void
model_interval_added (EogCollectionModel *model, GList *id_list, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	ItemUpdate *update;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	g_message ("model_interval_added called\n");

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;

	update = g_new0 (ItemUpdate, 1);
	update->hint = ITEM_ADDED;
	update->id_list = id_list;
	
	priv->item_update_list = g_list_append (priv->item_update_list, update);

	request_update (wlist);
}

/* Handler for the interval_changed signal from models */
static void
model_interval_removed (EogCollectionModel *model, GList *id_list, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	ItemUpdate *update;

	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (data != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	g_message ("model_interval_removed called\n");

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;

	update = g_new0 (ItemUpdate, 1);
	update->hint = ITEM_REMOVED;
	update->id_list = id_list;
	
	priv->item_update_list = g_list_append (priv->item_update_list, update);

	request_update (wlist);
}




/* Set model handler for the wrapped list view */
void
eog_wrap_list_set_model (EogWrapList *wlist, EogCollectionModel *model)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->model) {
		gtk_signal_disconnect (GTK_OBJECT (priv->model), priv->model_ids.interval_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (priv->model), priv->model_ids.interval_added_id);
		gtk_signal_disconnect (GTK_OBJECT (priv->model), priv->model_ids.interval_removed_id);

		priv->model_ids.interval_changed_id = 0;
		priv->model_ids.interval_added_id = 0;
		priv->model_ids.interval_removed_id = 0;
	}
	priv->model = NULL;

	if (model) {
		priv->model = model;
		gtk_object_ref (GTK_OBJECT (model));

		priv->model_ids.interval_changed_id = gtk_signal_connect (
			GTK_OBJECT (model), "interval_changed",
			GTK_SIGNAL_FUNC (model_interval_changed),
			wlist);

		priv->model_ids.interval_added_id = gtk_signal_connect (
			GTK_OBJECT (model), "interval_added",
			GTK_SIGNAL_FUNC (model_interval_added),
			wlist);

		priv->model_ids.interval_removed_id = gtk_signal_connect (
			GTK_OBJECT (model), "interval_removed",
			GTK_SIGNAL_FUNC (model_interval_removed),
			wlist);
	}

	priv->global_update_hints[GLOBAL_MODEL_CHANGED] = TRUE;

	request_update (wlist);
}

void
eog_wrap_list_set_factory (EogWrapList *wlist, GnomeListItemFactory *factory)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->factory) {
		gtk_object_unref (GTK_OBJECT (priv->factory));
	}
	priv->factory = NULL;

	if (factory) {
		priv->factory = factory;
	}

	priv->global_update_hints[GLOBAL_MODEL_CHANGED] = TRUE;

	request_update (wlist);
}



/**
 * eog_wrap_list_set_row_spacing:
 * @wlist: A wrapped list view.
 * @spacing: Spacing between rows in pixels.
 *
 * Sets the spacing between the rows of a wrapped list view.
 **/
void
eog_wrap_list_set_row_spacing (EogWrapList *wlist, guint spacing)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;
	priv->row_spacing = spacing;

	priv->global_update_hints[GLOBAL_SPACING_CHANGED] = TRUE;

	request_update (wlist);
}

/**
 * eog_wrap_list_set_col_spacing:
 * @wlist: A wrapped list view.
 * @spacing: Spacing between columns in pixels.
 *
 * Sets the spacing between the columns of a wrapped list view.
 **/
void
eog_wrap_list_set_col_spacing (EogWrapList *wlist, guint spacing)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;
	priv->col_spacing = spacing;

	priv->global_update_hints[GLOBAL_SPACING_CHANGED] = TRUE;

	request_update (wlist);
}

static 
void eog_wrap_list_clear (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	gtk_object_destroy (GTK_OBJECT (priv->item_group));
	priv->item_group =
		gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (wlist)),	
				       gnome_canvas_group_get_type (),
				       "x", 0.0,
				       "y", 0.0,
				       NULL);
						  
	g_hash_table_destroy (priv->item_table);
	priv->item_table = g_hash_table_new (NULL, NULL);
}

static void 
request_update (EogWrapList *wlist)
{
	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	g_message ("request_update called.");

	if ((wlist->priv->idle_handler_id == -1) &&
	    (!wlist->priv->is_updating))
		wlist->priv->idle_handler_id = gtk_idle_add ((GtkFunction) update, wlist);
}

static GList*
get_next_unique_id (GList *id_list, 
		    gint *id_range_start,
		    gint *id_range_end)
{
	gint value;

	if (id_list == NULL) return NULL;
	
	value = GPOINTER_TO_INT (id_list->data);
	
	if (value != EOG_MODEL_ID_RANGE) {
		*id_range_start = value;
		*id_range_end = value;
		return id_list->next;
	}

	/* handle range */
	id_list = id_list->next;
	g_assert (id_list != NULL);
	*id_range_start = GPOINTER_TO_INT (id_list->data);

	id_list = id_list->next;
	g_assert (id_list != NULL);
	*id_range_end = GPOINTER_TO_INT (id_list->data);
	
	return id_list->next;
}

static GnomeCanvasItem*
get_item_by_unique_id (EogWrapList *wlist,
		       gint unique_id)
{
	g_return_val_if_fail (wlist != NULL, NULL);
	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), NULL);
	
	return (GnomeCanvasItem*) g_hash_table_lookup (wlist->priv->item_table, 
						       GINT_TO_POINTER (unique_id));
}

static void 
do_item_changed_update (EogWrapList *wlist,
			GList *id_list,
			gboolean *layout_check_needed,
			gboolean *item_rearrangement_needed)
{
	gint id_range_start = EOG_MODEL_ID_NONE;
	gint id_range_end = EOG_MODEL_ID_NONE;

	*layout_check_needed = FALSE;
	*item_rearrangement_needed = FALSE;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	g_message ("do_item_changed_update called\n");

	if (id_list == NULL) return;
	if (wlist->priv->factory == NULL) return;

	while (id_list != NULL) {
		
		id_list = get_next_unique_id (id_list,
					      &id_range_start, 
					      &id_range_end);

		if (id_range_start != EOG_MODEL_ID_NONE) {
			gint id;
			for (id = id_range_start; id <= id_range_end; id++) {
				GnomeCanvasItem *item;
				item = get_item_by_unique_id (wlist, id);
				g_print ("update item id: %i\n", id);
 				gnome_list_item_factory_update_item (wlist->priv->factory,
								     wlist->priv->model,
								     item);
			}
		}
	}
}

static void
do_item_removed_update (EogWrapList *wlist,
			GList *id_list,
			gboolean *layout_check_needed,
			gboolean *item_rearrangement_needed)
{
	EogWrapListPrivate *priv;

	gint id_range_start = EOG_MODEL_ID_NONE;
	gint id_range_end = EOG_MODEL_ID_NONE;

	*layout_check_needed = FALSE;
	*item_rearrangement_needed = FALSE;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	g_message ("do_item_removed_update called\n");

	priv = wlist->priv;

	if (id_list == NULL) return;
	if (priv->factory == NULL) return;

	while (id_list != NULL) {
		
		id_list = get_next_unique_id (id_list,
					      &id_range_start, 
					      &id_range_end);

		if (id_range_start != EOG_MODEL_ID_NONE) {
			gint id;
			GnomeCanvasItem *item;			
			for (id = id_range_start; id <= id_range_end; id++) {
				item = get_item_by_unique_id (wlist, id);
				g_hash_table_remove (priv->item_table, 
						     GINT_TO_POINTER (id));
				gtk_object_destroy (GTK_OBJECT (item));
				priv->n_items--;
			}
		}
	}	
}

static void
do_item_added_update (EogWrapList *wlist,
		      GList *id_list,
		      gboolean *layout_check_needed,
		      gboolean *item_rearrangement_needed)
{
	EogWrapListPrivate *priv;

	gint id_range_start = EOG_MODEL_ID_NONE;
	gint id_range_end = EOG_MODEL_ID_NONE;

	*layout_check_needed = FALSE;
	*item_rearrangement_needed = FALSE;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	g_message ("do_item_added_update called\n");

	priv = wlist->priv;

	if (id_list == NULL) return;
	if (wlist->priv->factory == NULL) return;

	if (priv->item_group == NULL) 
		priv->item_group = 
			gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (wlist)),	
					       gnome_canvas_group_get_type (),
					       "x", 0.0,
					       "y", 0.0,
					       NULL);

	while (id_list != NULL) {
		
		id_list = get_next_unique_id (id_list,
					      &id_range_start, 
					      &id_range_end);

		if (id_range_start != EOG_MODEL_ID_NONE) {
			gint id;
			GnomeCanvasItem *item;
			for (id = id_range_start; id <= id_range_end; id++) {
				item = gnome_list_item_factory_create_item (priv->factory,
									    GNOME_CANVAS_GROUP (priv->item_group),
									    id);
				gnome_list_item_factory_update_item (priv->factory,
								     priv->model,
								     item);
			        g_hash_table_insert (priv->item_table, GINT_TO_POINTER (id),
						     item);
				priv->n_items++;
			}

		}
	}

	*layout_check_needed = TRUE;
	*item_rearrangement_needed = TRUE;
}

static gboolean
do_layout_check (EogWrapList *wlist)
{
	guint n_rows_new;
	guint n_cols_new;
	gint canvas_width;
	
	EogWrapListPrivate *priv;

	priv = wlist->priv;

	g_message ("do_layout_check called\n");

	/* get canvas width */
	canvas_width = GTK_WIDGET (wlist)->allocation.width;

	/* calculate new number of  columns/rows */
	n_cols_new = canvas_width / (120 /*priv->item_width*/ + priv->col_spacing);
	if (n_cols_new == 0) n_cols_new = 1;
	n_rows_new = priv->n_items / n_cols_new;
	n_rows_new = priv->n_items % n_cols_new ? n_rows_new++ : n_rows_new; 
	
	g_print ("  ** canvas width: %i\n", canvas_width);
	g_print ("  ** n_cols_new: %i\n", n_cols_new);
	g_print ("  ** n_rows_new: %i\n", n_rows_new);

	if (n_cols_new == priv->n_cols && n_rows_new == priv->n_rows)
		return FALSE;

	priv->n_cols = n_cols_new;
	priv->n_rows = n_rows_new;

	return TRUE;
}

static void 
calculate_item_position (EogWrapList *wlist, 
			 guint item_number,
			 double *world_x,
			 double *world_y)
{
	EogWrapListPrivate *priv;
	guint row;
	guint col;

	priv = wlist->priv;

	row = item_number / priv->n_cols;
	col = item_number % priv->n_cols;
		
	*world_x = col * (120 /*priv->item_width*/ + priv->col_spacing);
	*world_y = row * (120 /*priv->item_height*/ + priv->row_spacing);
}

typedef struct {
	EogWrapList *wlist;
	gint n;
} RearrangeData; 

static void
rearrange_single_item (gpointer key, gpointer value, RearrangeData *data)
{
	GnomeCanvasItem *item;
	double x1, x2, y1, y2;
	double x3, y3;
	double xoff, yoff;
	
	item = (GnomeCanvasItem*) value;
	
	gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);
	
	calculate_item_position (data->wlist, data->n, &x3, &y3);
	
	xoff = x3-x1;
	yoff = y3-y1;
		
	if (xoff || yoff)
		gnome_canvas_item_move (item, xoff, yoff);
	
	data->n++;
}

static void
do_item_rearrangement (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
        RearrangeData data;
	double sr_width, sr_height;

	data.wlist = wlist;
	data.n = 0;

	g_message ("do_item_rearrangement called\n");

	priv = wlist->priv;

	g_hash_table_foreach (priv->item_table, 
			      (GHFunc) rearrange_single_item, 
			      &data);

	/* set new canvas scroll region */
	/* FIXME: don't use hardcoded item dimensions */
	sr_width =  priv->n_cols * (120 /*priv->item_width*/ + priv->col_spacing);
	sr_height = priv->n_rows * (120 /*priv->item_height*/ + priv->row_spacing);
 	gnome_canvas_set_scroll_region (GNOME_CANVAS (wlist), 
					0.0, 0.0,
					sr_width, sr_height);
}

static void 
update (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
	GList *item_update;
	gboolean layout_check_needed = FALSE;
	gboolean item_rearrangement_needed = FALSE;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	g_message ("update called\n");

	priv = wlist->priv;

        /* remove idle function */ 
	priv->is_updating = TRUE;
	gtk_idle_remove (wlist->priv->idle_handler_id);
	priv->idle_handler_id = -1;
	
	/* handle global updates */
	if (priv->global_update_hints[GLOBAL_FACTORY_CHANGED]) {
		priv->global_update_hints[GLOBAL_FACTORY_CHANGED] = FALSE;
		
	} else if (priv->global_update_hints[GLOBAL_MODEL_CHANGED]) {
		priv->global_update_hints[GLOBAL_MODEL_CHANGED] = FALSE;
		
	} else if (priv->global_update_hints[GLOBAL_SPACING_CHANGED] ||
		   priv->global_update_hints[GLOBAL_SIZE_CHANGED]) {
		layout_check_needed = TRUE;
		item_rearrangement_needed = TRUE;

		priv->global_update_hints[GLOBAL_SPACING_CHANGED] = FALSE;
		priv->global_update_hints[GLOBAL_SIZE_CHANGED] = FALSE;
	}

	item_update = priv->item_update_list;
	
	while (item_update) {
		ItemUpdate *update = (ItemUpdate*) item_update->data;

		switch (update->hint) {
		case ITEM_CHANGED:
			do_item_changed_update (wlist,
						update->id_list,
						&layout_check_needed, 
						&item_rearrangement_needed);
			break;

		case ITEM_ADDED:
			do_item_added_update (wlist,
					      update->id_list,
					      &layout_check_needed, 
					      &item_rearrangement_needed);
			break;

		case ITEM_REMOVED:
			do_item_removed_update (wlist,
						update->id_list,
						&layout_check_needed, 
						&item_rearrangement_needed);
			break;
		default:
			g_assert_not_reached ();
		}

		/* free id list */
		g_list_free (update->id_list);
		g_free (update);

		item_update = g_list_next (item_update);
	}
	
	/* free update list */
	if (priv->item_update_list)
		g_list_free (priv->item_update_list);
	priv->item_update_list = NULL;

	if (layout_check_needed && do_layout_check (wlist)) 
		item_rearrangement_needed = TRUE;
	
	if (item_rearrangement_needed) {
 		do_item_rearrangement (wlist);
	}

	priv->is_updating = FALSE;

}
