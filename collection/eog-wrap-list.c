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
#include <glib/gmain.h>
#include <libgnome/gnome-macros.h>

#include "eog-wrap-list.h"
#include "eog-collection-marshal.h"
#include <math.h>



/* Used to hold signal handler IDs for models */
typedef struct {
	guint interval_changed_id;
	guint interval_added_id;
	guint interval_removed_id;
} ModelSignalIDs;

enum {
	RIGHT_CLICK,
	DOUBLE_CLICK,
	LAST_SIGNAL
};

static guint eog_wrap_list_signals [LAST_SIGNAL];

enum {
	GLOBAL_SIZE_CHANGED,
	GLOBAL_SPACING_CHANGED,
	GLOBAL_FACTORY_CHANGED,
	GLOBAL_MODEL_CHANGED,
	GLOBAL_LAYOUT_MODE_CHANGED,
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

	/* Sorted list of items */
	GSList *view_order;
	
	/* Factory to use. */
	EogItemFactory *factory;

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

	gboolean is_updating;

	/* unique IDs to update */
	GList *item_update_list;

	/* last id of thumbnail the user clicked */
	guint last_id_clicked;
};

static void eog_wrap_list_class_init (EogWrapListClass *class);
static void eog_wrap_list_instance_init (EogWrapList *wlist);
static void eog_wrap_list_dispose (GObject *object);
static void eog_wrap_list_finalize (GObject *object);

static void eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation);

static void request_update (EogWrapList *wlist);
static void do_update (EogWrapList *wlist);
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
	priv->model = NULL;
	priv->factory = NULL;
	priv->row_spacing = 0;
	priv->col_spacing = 0;
	priv->idle_handler_id = -1;
	priv->is_updating = FALSE;
	priv->n_items = 0;
	priv->lm = EOG_LAYOUT_MODE_VERTICAL;
	priv->n_rows = 0;
	priv->n_cols = 0;
	priv->last_id_clicked = -1;
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

	if (priv->factory)
		g_object_unref (G_OBJECT (priv->factory));
	priv->factory = NULL;

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

	gtk_widget_set_events (GTK_WIDGET (wlist), GDK_ALL_EVENTS_MASK);
	g_signal_connect_after (G_OBJECT (wlist), 
				"button-press-event",
				G_CALLBACK (handle_canvas_click),
				wlist);
}


GtkWidget*
eog_wrap_list_new (void)
{
	GtkWidget *wlist;

	wlist = gtk_widget_new (eog_wrap_list_get_type (), NULL);

	eog_wrap_list_construct (EOG_WRAP_LIST (wlist));
	
	return wlist;
}

static GnomeCanvasItem*
get_item_by_unique_id (EogWrapList *wlist,
		       gint unique_id)
{
	GnomeCanvasItem *item;

	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), NULL);
	
	item = g_hash_table_lookup (wlist->priv->item_table,
				    GINT_TO_POINTER (unique_id));
	if (!item)
		g_warning ("Could not find item %i!", unique_id);

	return (item);
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

static gint
handle_canvas_click (GnomeCanvas *canvas, GdkEventButton *event, gpointer data)
{
	EogWrapList *wlist;
	EogCollectionModel *model;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_WRAP_LIST (data), FALSE);

	wlist = EOG_WRAP_LIST (data);
	model = wlist->priv->model;
	if (model == NULL) return FALSE;

	eog_collection_model_set_select_status_all (model, FALSE);
	wlist->priv->last_id_clicked = -1;
	
	return TRUE;
}

static gint
handle_item_event (GnomeCanvasItem *item, GdkEvent *event,  EogWrapList *wlist) 
{
	EogWrapListPrivate *priv;
	EogCollectionModel *model;
	gint id;
	gboolean ret_val;

	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), FALSE);

	priv = wlist->priv;
	model = priv->model;
	if (model == NULL) return FALSE;

	id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "ImageID"));

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (event->button.button) {
		case 3:
			/* Right click */
			eog_collection_model_set_select_status(model, id, TRUE);
			ret_val = FALSE;
			g_signal_emit (GTK_OBJECT (wlist),
				       eog_wrap_list_signals [RIGHT_CLICK], 0,
				       id, event, &ret_val);
			g_signal_stop_emission_by_name (G_OBJECT (item->canvas),
							"button-press-event");
			return (ret_val);
		case 2:
			/* Middle button */
			g_warning ("Implement D&D!");
			break;
		case 1:
			/* Left click */
			if (event->button.state & GDK_SHIFT_MASK) {
				/* shift key pressed */
				if (wlist->priv->last_id_clicked == -1)
					eog_collection_model_toggle_select_status (model, id);
				else {
					GnomeCanvasItem *prev_item;
					GSList *node = NULL;
					gint prev_n, n;
					gint i;
				
					prev_item = get_item_by_unique_id (
						wlist, priv->last_id_clicked);
					prev_n = get_item_view_position (
							wlist, prev_item);
					n = get_item_view_position(wlist, item);

					/*
					 * assert that always prev_n <= n
					 * is valid
					 */
					if (n < prev_n) {
						gint tmp;
						tmp = prev_n;
						prev_n = n - 1;
						n = tmp - 1;
					}

					/* init variables */
					if (prev_n == -1) {
						/*
						 * happens if n == 0 and
						 * prev_n > 0
						 */
						node = priv->view_order;
						i = 0;
					} else {
						node = g_slist_nth (
							priv->view_order,
							prev_n);
						node = node->next;
						i = ++prev_n;
					}
				
					/*
					 * change select status for items in
					 * range (prev_n, n]
					 */
					while (node && (i <= n)) {
						GnomeCanvasItem *item;
						gint id;
					
						item = GNOME_CANVAS_ITEM (
								node->data);
						id = GPOINTER_TO_INT (
							gtk_object_get_data (
							GTK_OBJECT (item),
							"ImageID"));
						eog_collection_model_toggle_select_status (model, id);

						node = node->next;
						i++;
					} 
				}
			} else if (event->button.state & GDK_CONTROL_MASK) {
				/* control key pressed */
		
				/* add item with id to selection*/
				eog_collection_model_toggle_select_status (
								model, id);
			} else {
				/* clear selection */
				eog_collection_model_set_select_status_all (
								model, FALSE);

				/* select only item with id */
				eog_collection_model_toggle_select_status (
								model, id);
			}

			wlist->priv->last_id_clicked = id;

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
		return FALSE;
	}

	return TRUE;
}

#if 0
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
#endif

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

#ifdef COLLECTION_DEBUG
	g_message ("model_interval_changed called\n");
#endif

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

#ifdef COLLECTION_DEBUG
	g_message ("model_interval_added called\n");
#endif

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
model_interval_removed (EogCollectionModel *model, GList *id_list,
			EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
	ItemUpdate *update;

	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));
	priv = wlist->priv;

#ifdef COLLECTION_DEBUG
	g_message ("model_interval_removed called\n");
#endif

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
		g_signal_handler_disconnect (G_OBJECT (priv->model), priv->model_ids.interval_changed_id);
		g_signal_handler_disconnect (G_OBJECT (priv->model), priv->model_ids.interval_added_id);
		g_signal_handler_disconnect (G_OBJECT (priv->model), priv->model_ids.interval_removed_id);

		priv->model_ids.interval_changed_id = 0;
		priv->model_ids.interval_added_id = 0;
		priv->model_ids.interval_removed_id = 0;
	}
	priv->model = NULL;

	if (model) {
		priv->model = model;
		g_object_ref (G_OBJECT (model));

		priv->model_ids.interval_changed_id = g_signal_connect (
			G_OBJECT (model), "interval_changed",
			G_CALLBACK (model_interval_changed),
			wlist);

		priv->model_ids.interval_added_id = g_signal_connect (
			G_OBJECT (model), "interval_added",
			G_CALLBACK (model_interval_added),
			wlist);

		priv->model_ids.interval_removed_id = g_signal_connect (
			G_OBJECT (model), "interval_removed",
			G_CALLBACK (model_interval_removed),
			wlist);
	}

	priv->global_update_hints[GLOBAL_MODEL_CHANGED] = TRUE;

	request_update (wlist);
}

void
eog_wrap_list_set_factory (EogWrapList *wlist, EogItemFactory *factory)
{
	EogWrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->factory) {
		g_object_unref (G_OBJECT (priv->factory));
	}
	priv->factory = NULL;

	if (factory) {
		priv->factory = factory;
		eog_item_factory_get_item_size (priv->factory,
						&priv->item_width,
						&priv->item_height);
	}

	priv->global_update_hints[GLOBAL_MODEL_CHANGED] = TRUE;

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

void 
eog_wrap_list_set_background_color (EogWrapList *wlist, GdkColor *color)
{
	
	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));
	g_return_if_fail (color != NULL);

	/* try to alloc color */
	if(gdk_color_alloc (gdk_colormap_get_system (), color))
	{
		GtkStyle *style;
		style = gtk_style_copy (gtk_widget_get_style (GTK_WIDGET (wlist)));

		/* set new style */
		style->bg[GTK_STATE_NORMAL] = *color;
		gtk_widget_set_style (GTK_WIDGET (wlist), style);
	}
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

#if 0
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
	priv->item_table = g_hash_table_new ((GHashFunc) g_direct_hash, 
					     (GCompareFunc) g_direct_equal);
}
#endif

static void 
request_update (EogWrapList *wlist)
{
	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

#ifdef COLLECTION_DEBUG
	g_message ("request_update called.");
#endif

	if ((wlist->priv->idle_handler_id == -1) &&
	    (!wlist->priv->is_updating))
		wlist->priv->idle_handler_id = gtk_idle_add (
					(GtkFunction) do_update, wlist);
}

static gint 
compare_item_caption (const GnomeCanvasItem *item1, const GnomeCanvasItem *item2)
{
	gchar *cap1;
	gchar *cap2;

	cap1 = (gchar*) g_object_get_data (G_OBJECT (item1), "Caption");
	cap2 = (gchar*) g_object_get_data (G_OBJECT (item2), "Caption");
	
	return g_strcasecmp (cap1, cap2);
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

#ifdef COLLECTION_DEBUG
	g_message ("do_item_changed_update called\n");
#endif

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
#ifdef COLLECTION_DEBUG
				g_print ("update item id: %i\n", id);
#endif
 				eog_item_factory_update_item (wlist->priv->factory,
							      wlist->priv->model,
							      item, EOG_ITEM_UPDATE_ALL);
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

	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));
	g_return_if_fail (id_list != NULL);
	g_return_if_fail (layout_check_needed != NULL);
	g_return_if_fail (item_rearrangement_needed != NULL);

	*layout_check_needed = FALSE;
	*item_rearrangement_needed = FALSE;

#ifdef COLLECTION_DEBUG
	g_message ("do_item_removed_update called\n");
#endif

	priv = wlist->priv;

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
				priv->view_order = g_slist_remove (
						priv->view_order, item); 
				gtk_object_destroy (GTK_OBJECT (item));
				priv->n_items--;
			}
		}
		*item_rearrangement_needed = TRUE;
		*layout_check_needed = TRUE;
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

#ifdef COLLECTION_DEBUG
	g_message ("do_item_added_update called\n");
#endif

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
			CImage *img;
			for (id = id_range_start; id <= id_range_end; id++) {
#ifdef COLLECTION_DEBUG
				g_print ("item_added: %i\n", id);
#endif
				item = eog_item_factory_create_item (priv->factory,
								     GNOME_CANVAS_GROUP (priv->item_group),
								     id);
				eog_item_factory_update_item (priv->factory,
							      priv->model,
							      item, EOG_ITEM_UPDATE_ALL);
			        g_hash_table_insert (priv->item_table, GINT_TO_POINTER (id),
						     item);
				priv->n_items++;
				g_signal_connect (G_OBJECT (item),
						  "event",
						  G_CALLBACK (handle_item_event),
						  (gpointer) wlist);
				g_object_set_data (G_OBJECT (item),
						   "ImageID", GINT_TO_POINTER (id));

				img = eog_collection_model_get_image (priv->model, id);
				g_object_set_data (G_OBJECT (item),
						   "Caption", cimage_get_caption (img));
				priv->view_order = 
					g_slist_insert_sorted (priv->view_order,
							       item,
							       (GCompareFunc) compare_item_caption);
			}

		}
	}

	*layout_check_needed = TRUE;
	*item_rearrangement_needed = TRUE;
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

#ifdef COLLECTION_DEBUG
	g_message ("do_layout_check called\n");
#endif

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

#ifdef COLLECTION_DEBUG	
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

#ifdef COLLECTION_DEBUG
	g_message ("do_item_rearrangement called\n");
#endif

	priv = wlist->priv;

	g_slist_foreach (priv->view_order, 
			 (GFunc) rearrange_single_item, 
			 &data);

	/* set new canvas scroll region */
	sr_width =  priv->n_cols * (priv->item_width + priv->col_spacing) - priv->col_spacing;
	sr_height = priv->n_rows * (priv->item_height + priv->row_spacing) - priv->row_spacing;
 	gnome_canvas_set_scroll_region (GNOME_CANVAS (wlist), 
					0.0, 0.0,
					sr_width, sr_height);
}

static void 
do_update (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
	GList *item_update;
	gboolean layout_check_needed = FALSE;
	gboolean item_rearrangement_needed = FALSE;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

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
	} else if (priv->global_update_hints[GLOBAL_LAYOUT_MODE_CHANGED]) {
		layout_check_needed = TRUE;
		priv->global_update_hints[GLOBAL_LAYOUT_MODE_CHANGED] = FALSE;
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
