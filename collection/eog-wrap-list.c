/* Eog Of Gnome - view of the image collection
 *
 * Copyright (C) 2001 The Free Software Foundation
 *
 * Author: Jens Finke <jens@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include <math.h>
#include <glib/gmain.h>
#include <libgnome/gnome-macros.h>
#include <gdk/gdkkeysyms.h>

#include "eog-wrap-list.h"
#include "eog-collection-marshal.h"
#include "eog-image.h"
#include "eog-collection-item.h"

#define COLLECTION_DEBUG 0


/* Used to hold signal handler IDs for models */
typedef enum {
	MODEL_SIGNAL_PREPARED,
	MODEL_SIGNAL_IMAGE_ADDED,
	MODEL_SIGNAL_IMAGE_REMOVED,
	MODEL_SIGNAL_LAST
} ModelSignal;

enum {
	SELECTION_CHANGED,
	RIGHT_CLICK,
	DOUBLE_CLICK,
	LAST_SIGNAL
};

static guint eog_wrap_list_signals [LAST_SIGNAL];

enum {
	GLOBAL_SIZE_CHANGED,
	GLOBAL_SPACING_CHANGED,
	GLOBAL_LAYOUT_MODE_CHANGED,
	GLOBAL_HINT_LAST
};

typedef struct {
	ModelSignal  hint;
	GQuark       id;
} ItemUpdate;

struct _EogWrapListPrivate {
	/* List of all items currently in the view. */
	GHashTable *item_table;

	/* Sorted list of items */
	GList *view_order;

	/* list of all selected items */
	GList *selected_items;

	/* number of selected items to prevent a slow
	 * g_list_length */
	int n_selected_items;
	
	/* Model to use. */
	EogCollectionModel *model;

	/* Signal connection IDs for the models */
	int model_ids[MODEL_SIGNAL_LAST];
	
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

	/* Layout mode to use for row/col calculating. */
	EogLayoutMode lm;

	/* Number of rows. */
	guint n_rows;

	/* Number of columns. */
	guint n_cols;

	/* Id for the idle handler.*/
	gint idle_handler_id;

	/* Update hints */
	gboolean global_update_hints [GLOBAL_HINT_LAST];

	/* last id of thumbnail the user clicked */
	GnomeCanvasItem *last_item_clicked;
};

static void eog_wrap_list_class_init (EogWrapListClass *class);
static void eog_wrap_list_instance_init (EogWrapList *wlist);
static void eog_wrap_list_dispose (GObject *object);
static void eog_wrap_list_finalize (GObject *object);

static void eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean eog_wrap_list_key_press_cb (GtkWidget *widget, GdkEventKey *event);

static void request_update (EogWrapList *wlist);
static gboolean do_update (EogWrapList *wlist);
static gint handle_canvas_click (GnomeCanvas *canvas, GdkEventButton *event, gpointer data);

GNOME_CLASS_BOILERPLATE (EogWrapList,
			 eog_wrap_list,
			 GnomeCanvas,
			 GNOME_TYPE_CANVAS);

/* Class initialization function for the abstract wrapped list view */
static void
eog_wrap_list_class_init (EogWrapListClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	object_class->dispose = eog_wrap_list_dispose;
	object_class->finalize = eog_wrap_list_finalize;

	widget_class->size_allocate = eog_wrap_list_size_allocate;
	widget_class->key_press_event = eog_wrap_list_key_press_cb;

	eog_wrap_list_signals [SELECTION_CHANGED] = 
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS(object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogWrapListClass, selection_changed),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	eog_wrap_list_signals [RIGHT_CLICK] = 
		g_signal_new ("right_click",
			      G_TYPE_FROM_CLASS(object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogWrapListClass, right_click),
			      NULL,
			      NULL,
			      eog_collection_marshal_BOOLEAN__INT_POINTER,
			      G_TYPE_BOOLEAN,
			      2,
			      G_TYPE_INT,
			      G_TYPE_POINTER);
	eog_wrap_list_signals [DOUBLE_CLICK] = 
		g_signal_new ("double_click",
			      G_TYPE_FROM_CLASS(object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EogWrapListClass, double_click),
			      NULL,
			      NULL,
			      eog_collection_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
}

/* object initialization function for the abstract wrapped list view */
static void
eog_wrap_list_instance_init (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;

	priv = g_new0 (EogWrapListPrivate, 1);
	wlist->priv = priv;
	
	priv->item_table = NULL;
	priv->view_order = NULL;
	priv->selected_items = NULL;
	priv->n_selected_items = 0;
	priv->model = NULL;
	priv->row_spacing = 0;
	priv->col_spacing = 0;
	priv->idle_handler_id = -1;
	priv->n_items = 0;
	priv->lm = EOG_LAYOUT_MODE_VERTICAL;
	priv->n_rows = 0;
	priv->n_cols = 0;
	priv->last_item_clicked = NULL;
	priv->item_width = -1;
	priv->item_height = -1;
}

/* Destroy handler for the abstract wrapped list view */
static void
eog_wrap_list_dispose (GObject *object)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (object));

	wlist = EOG_WRAP_LIST (object);
	priv = wlist->priv;

	if (priv->model)
		g_object_unref (G_OBJECT (priv->model));
	priv->model = NULL;

	/* FIXME: free the items and item array */
	
	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_wrap_list_finalize (GObject *object)
{
	EogWrapList *wlist;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (object));

	wlist = EOG_WRAP_LIST (object);
	if (wlist->priv)
		g_free (wlist->priv);
	wlist->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
eog_wrap_list_construct (EogWrapList *wlist)
{
	wlist->priv->item_table = g_hash_table_new ((GHashFunc) g_direct_hash, 
						    (GCompareFunc) g_direct_equal);

	g_signal_connect_after (G_OBJECT (wlist), 
				"button-press-event",
				G_CALLBACK (handle_canvas_click),
				wlist);
}


GtkWidget*
eog_wrap_list_new (void)
{
	GtkWidget *wlist;

	wlist = gtk_widget_new (eog_wrap_list_get_type (), 
				"can-focus", TRUE,
				"events", 
				GDK_EXPOSURE_MASK | 
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_KEY_PRESS_MASK,
				NULL);

	eog_wrap_list_construct (EOG_WRAP_LIST (wlist));
	
	return wlist;
}


static gint
get_item_view_position (EogWrapList *wlist, GnomeCanvasItem *item)
{
	EogWrapListPrivate *priv;
	double x1, y1, x2, y2;
	gint row, col, n;
	
	priv = wlist->priv;
	gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);

	col = x1 / (priv->item_width + priv->col_spacing);
	row = y1 / (priv->item_height + priv->row_spacing);

	n = col + priv->n_cols * row;

	return n;
}

static gboolean
set_select_status (EogWrapList *wlist, EogCollectionItem *item, gboolean state)
{
	EogWrapListPrivate *priv;

	priv = wlist->priv;

	if (state && !eog_collection_item_is_selected (item))  {
		eog_collection_item_set_selected (item, state);
		
		priv->selected_items = g_list_append (priv->selected_items, g_object_ref (item));
		priv->n_selected_items++;
		return TRUE;
	}
	else if (!state && eog_collection_item_is_selected (item)) {
		eog_collection_item_set_selected (item, state);

		priv->selected_items = g_list_remove (priv->selected_items, item);
		g_object_unref (item);
		priv->n_selected_items--;
		g_assert (priv->n_selected_items >= 0);
		return TRUE;
	}

	return FALSE;
}

static void
toggle_select_status (EogWrapList *wlist, EogCollectionItem *item)
{
	set_select_status (wlist, item, !eog_collection_item_is_selected (item));
}


static void
deselect_all (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
	GList *it;

	priv = wlist->priv;

	for (it = priv->selected_items; it != NULL; it = it->next) {
		EogCollectionItem *item = EOG_COLLECTION_ITEM (it->data);
		eog_collection_item_set_selected (item, FALSE);
		g_object_unref (item);
	}

	priv->n_selected_items = 0;
	
	if (priv->selected_items != NULL) {
		g_list_free (priv->selected_items);
		priv->selected_items = NULL;
	}
}

static gint
handle_canvas_click (GnomeCanvas *canvas, GdkEventButton *event, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WRAP_LIST (data), FALSE);

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;

	if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (wlist)))
		gtk_widget_grab_focus (GTK_WIDGET (wlist));

	if (priv->n_selected_items > 1) {
		EogCollectionItem *item;

		item = EOG_COLLECTION_ITEM (priv->selected_items->data);

		deselect_all (wlist);
		set_select_status (wlist, item, TRUE);
		
		g_signal_emit (G_OBJECT (wlist), eog_wrap_list_signals [SELECTION_CHANGED], 0);
		wlist->priv->last_item_clicked = GNOME_CANVAS_ITEM (item);
	}
	
	return TRUE;
}

static gint
handle_item_event (GnomeCanvasItem *item, GdkEvent *event,  EogWrapList *wlist) 
{
	EogWrapListPrivate *priv;
	EogCollectionModel *model;
	EogCollectionItem *eci;
	GQuark id = 0;
	gboolean ret_val = TRUE;
	gboolean selection_changed = FALSE;

	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), FALSE);

	if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (wlist)))
		gtk_widget_grab_focus (GTK_WIDGET (wlist));
	
	priv = wlist->priv;
	model = priv->model;
	if (model == NULL) return FALSE;

	eci = EOG_COLLECTION_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (event->button.button) {
		case 3:
			/* Right click */
			selection_changed = set_select_status (wlist, eci, TRUE);
			g_signal_emit (GTK_OBJECT (wlist),
				       eog_wrap_list_signals [RIGHT_CLICK], 0,
				       id, event, &ret_val);
			g_signal_stop_emission_by_name (G_OBJECT (item->canvas),
							"button-press-event");
			break;
		case 2:
			/* Middle button */
			g_warning ("Implement D&D!");
			break;
		case 1:
			/* Left click */
			if (event->button.state & GDK_SHIFT_MASK) {
				/* shift key pressed */
				if (wlist->priv->last_item_clicked == NULL) {
					toggle_select_status (wlist, eci);
					selection_changed = TRUE;
				}
				else {
					int pos; 
					int prev_pos;
					GList *start_node;
					GList *end_node;
					GList *node;
					gboolean changed;

					pos = get_item_view_position (wlist, item);
					prev_pos = get_item_view_position (wlist, priv->last_item_clicked);

					if (pos > prev_pos) {
						start_node = g_list_find (priv->view_order, priv->last_item_clicked);
						end_node = g_list_find (priv->view_order, item);
					}
					else {
						start_node = g_list_find (priv->view_order, item);
						end_node = g_list_find (priv->view_order, priv->last_item_clicked);
					}

					for (node = start_node; node != end_node->next; node = node->next) {
						changed = set_select_status (wlist, EOG_COLLECTION_ITEM (node->data), TRUE);
						selection_changed |= changed;
					}

				}
			} else if (event->button.state & GDK_CONTROL_MASK) {
				/* control key pressed */
		
				/* add item with id to selection*/
				toggle_select_status (wlist, eci);
				selection_changed = TRUE;
			} else {
				/* clear selection */
				deselect_all (wlist);

				/* select only item with id */
				toggle_select_status (wlist, eci);
				selection_changed = TRUE;
			}

			wlist->priv->last_item_clicked = item;

			/*
			 * stop further event handling through
			 * handle_canvas_click
			 */
			g_signal_stop_emission_by_name (G_OBJECT (item->canvas),
							"button-press-event");
		}

		break;
			
	case GDK_2BUTTON_PRESS:
		/* stop further event handling through handle_canvas_click */
		g_signal_stop_emission_by_name (G_OBJECT (item->canvas), "button-press-event");

		g_signal_emit (G_OBJECT (wlist), 
			       eog_wrap_list_signals [DOUBLE_CLICK], 0,
			       id);
		break;
		
	default:
		ret_val = FALSE;
	}

	if (selection_changed) {
		g_signal_emit (G_OBJECT (wlist), eog_wrap_list_signals [SELECTION_CHANGED], 0);
	}

	return ret_val;
}

/* Size_allocate handler for the abstract wrapped list view */
static void
eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	wlist = EOG_WRAP_LIST (widget);
	priv = wlist->priv;

	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	priv->global_update_hints[GLOBAL_SIZE_CHANGED] = TRUE;

	request_update (wlist);
}       

static gboolean 
eog_wrap_list_key_press_cb (GtkWidget *widget, GdkEventKey *event)
{
	switch (event->keyval) {
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
		return TRUE;
		break;
	};

	return GNOME_CALL_PARENT_WITH_DEFAULT (GTK_WIDGET_CLASS, key_press_event, (widget, event), FALSE);
}

static gboolean
add_image (EogCollectionModel *model, EogImage *image, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	GnomeCanvasItem *item;
	
	g_return_val_if_fail (EOG_IS_WRAP_LIST (data), FALSE);

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;
	
	item = eog_collection_item_new (gnome_canvas_root (GNOME_CANVAS (wlist)), image);
	g_signal_connect (G_OBJECT (item), "event", G_CALLBACK (handle_item_event), wlist);

	priv->view_order = g_list_prepend (priv->view_order, item);

	return TRUE;
}

static void
model_prepared (EogCollectionModel *model, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	GList *it;
	
	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;
	
	g_assert (priv->view_order == NULL);

	eog_collection_model_foreach (model, add_image, EOG_WRAP_LIST (data));

	priv->view_order = g_list_reverse (priv->view_order);
	priv->n_items = g_list_length (priv->view_order);

	request_update (wlist);
	
	for (it = priv->view_order; it != NULL; it = it->next) {
		eog_collection_item_load (EOG_COLLECTION_ITEM (it->data));
	}

	if (priv->view_order != NULL) {
		deselect_all (wlist);
		if (set_select_status (wlist, EOG_COLLECTION_ITEM (priv->view_order->data), TRUE)) {
			g_signal_emit (G_OBJECT (wlist), eog_wrap_list_signals [SELECTION_CHANGED], 0);
		}
	}
}

/* Handler for the interval_added signal from models */
static void
model_image_added (EogCollectionModel *model, EogImage *image, int position, gpointer data)
{
	GnomeCanvasItem *item;
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;

	/* insert image */
	item = eog_collection_item_new (gnome_canvas_root (GNOME_CANVAS (wlist)), image);
	g_signal_connect (G_OBJECT (item), "event", G_CALLBACK (handle_item_event), wlist);

	priv->view_order = g_list_insert (priv->view_order, item, position);
	priv->n_items = g_list_length (priv->view_order);

	request_update (wlist);
	
	eog_collection_item_load (EOG_COLLECTION_ITEM (item));
}

/* Handler for the interval_changed signal from models */
static void
model_image_removed (EogCollectionModel *model, GQuark id, gpointer data)
{
#if 0
	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	g_message ("model_interval_removed called\n");

	/* FIXME: implement this. */
#endif
}



/* Set model handler for the wrapped list view */
void
eog_wrap_list_set_model (EogWrapList *wlist, EogCollectionModel *model)
{
	EogWrapListPrivate *priv;
	int i;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->model) {
		for (i = 0; i < MODEL_SIGNAL_LAST; i++) {
			g_signal_handler_disconnect (G_OBJECT (priv->model), priv->model_ids[i]);
			priv->model_ids[i] = 0;
		}
	}
	priv->model = NULL;

	if (model) {
		priv->model = model;
		g_object_ref (G_OBJECT (model));

		priv->model_ids[MODEL_SIGNAL_PREPARED] = g_signal_connect (
			G_OBJECT (model), "prepared",
			G_CALLBACK (model_prepared),
			wlist);

		priv->model_ids[MODEL_SIGNAL_IMAGE_ADDED] = g_signal_connect (
			G_OBJECT (model), "image-added",
			G_CALLBACK (model_image_added),
			wlist);

		priv->model_ids[MODEL_SIGNAL_IMAGE_REMOVED] = g_signal_connect (
			G_OBJECT (model), "image-removed",
			G_CALLBACK (model_image_removed),
			wlist);
	}

	request_update (wlist);
}

void 
eog_wrap_list_set_layout_mode (EogWrapList *wlist, EogLayoutMode lm)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (lm == priv->lm) return;

	priv->lm = lm;
	priv->global_update_hints[GLOBAL_LAYOUT_MODE_CHANGED] = TRUE;

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

static void 
request_update (EogWrapList *wlist)
{
	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

#if COLLECTION_DEBUG
	g_message ("request_update called.");
#endif

	if (wlist->priv->idle_handler_id == -1)
	{
		wlist->priv->idle_handler_id = g_idle_add ((GSourceFunc) do_update, wlist);
	}
}

static gboolean
do_layout_check (EogWrapList *wlist)
{
	unsigned int n_rows_new = 0;
	unsigned int n_cols_new = 0;
	gint cw;
	gint ch;
	
	EogWrapListPrivate *priv;

	priv = wlist->priv;

#if COLLECTION_DEBUG
	g_message ("do_layout_check called");
#endif

	if (priv->item_width == -1 || priv->item_height == -1) {
		if (priv->view_order != NULL) {
			double x1, y1, x2, y2;

			gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (priv->view_order->data), &x1, &y1, &x2, &y2);
			priv->item_width = (int) x2 - x1; 
			priv->item_height = (int) y2 - y1;
		}
		else {
			return FALSE;
		}
	}


	/* get canvas width */
	cw = GTK_WIDGET (wlist)->allocation.width;
	ch = GTK_WIDGET (wlist)->allocation.height;

	/* calculate new number of  columns/rows */
	switch (priv->lm) {
	case EOG_LAYOUT_MODE_VERTICAL:
		n_cols_new = cw / (priv->item_width + priv->col_spacing);
		if (n_cols_new == 0) n_cols_new = 1;
		n_rows_new = priv->n_items / n_cols_new;
		n_rows_new = priv->n_items % n_cols_new ? n_rows_new++ : n_rows_new; 
		break;

	case EOG_LAYOUT_MODE_HORIZONTAL:
		n_rows_new = ch / (priv->item_height + priv->row_spacing);
		if (n_rows_new == 0) n_rows_new = 1;
		n_cols_new = priv->n_items / n_rows_new;
		n_cols_new = priv->n_items % n_rows_new ? n_cols_new++ : n_cols_new; 
		break;

	case EOG_LAYOUT_MODE_RECTANGLE:
		n_rows_new = n_cols_new = sqrt (priv->n_items);
		if (n_rows_new * n_cols_new < priv->n_items) {
			if ((n_rows_new+1) * n_cols_new < priv->n_items)
				n_rows_new = n_cols_new = n_rows_new + 1;
			else 
				n_rows_new = n_rows_new + 1;
		}
		break;

	default:
		g_assert_not_reached ();
	}

#if COLLECTION_DEBUG	
	g_print ("  ** canvas width: %i\n",cw);
	g_print ("  ** n_cols_new: %i\n", n_cols_new);
	g_print ("  ** n_rows_new: %i\n", n_rows_new);
#endif

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
		
	*world_x = col * (priv->item_width + priv->col_spacing);
	*world_y = row * (priv->item_height + priv->row_spacing);
}

typedef struct {
	EogWrapList *wlist;
	gint n;
} RearrangeData; 

static void
rearrange_single_item (gpointer value, RearrangeData *data)
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

#if COLLECTION_DEBUG
	g_message ("do_item_rearrangement called");
#endif

	priv = wlist->priv;

	g_list_foreach (priv->view_order, 
			(GFunc) rearrange_single_item, 
			 &data);

	/* set new canvas scroll region */
	sr_width =  priv->n_cols * (priv->item_width + priv->col_spacing) - priv->col_spacing;
	sr_height = priv->n_rows * (priv->item_height + priv->row_spacing) - priv->row_spacing;
 	gnome_canvas_set_scroll_region (GNOME_CANVAS (wlist), 
					0.0, 0.0,
					sr_width, sr_height);

#if COLLECTION_DEBUG
	g_message ("do_item_rearrangement leaved");
#endif
}

static gboolean
do_update (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
	gboolean layout_check_needed = FALSE;
	gboolean item_rearrangement_needed = FALSE;

	g_return_val_if_fail (wlist != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), FALSE);

	priv = wlist->priv;
	
	/* handle global updates */
	if (priv->global_update_hints [GLOBAL_SPACING_CHANGED] ||
	    priv->global_update_hints [GLOBAL_SIZE_CHANGED]) 
	{
		layout_check_needed = TRUE;
		item_rearrangement_needed = TRUE;
		
		priv->global_update_hints[GLOBAL_SPACING_CHANGED] = FALSE;
		priv->global_update_hints[GLOBAL_SIZE_CHANGED] = FALSE;
	} 
	else if (priv->global_update_hints [GLOBAL_LAYOUT_MODE_CHANGED]) 
	{
		layout_check_needed = TRUE;

		priv->global_update_hints[GLOBAL_LAYOUT_MODE_CHANGED] = FALSE;
	}

	
	if (layout_check_needed && do_layout_check (wlist)) 
		item_rearrangement_needed = TRUE;
	
	if (item_rearrangement_needed) {
 		do_item_rearrangement (wlist);
	}


	priv->idle_handler_id = -1;
	return FALSE;
}

int
eog_wrap_list_get_n_selected (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;

	priv = wlist->priv;
	
	return priv->n_selected_items;
}

EogImage* 
eog_wrap_list_get_first_selected_image (EogWrapList *wlist)
{
	EogImage *image = NULL;
	EogWrapListPrivate *priv;

	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), NULL);

	priv = wlist->priv;

	if (priv->selected_items != NULL) {
		EogCollectionItem *item;

		item = EOG_COLLECTION_ITEM (priv->selected_items->data);
		image = eog_collection_item_get_image (item);
	}

	return image;
}

GList* 
eog_wrap_list_get_selected_images (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;

	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), NULL);

	priv = wlist->priv;

	return priv->selected_items;
}
