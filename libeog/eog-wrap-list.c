/* Eog Of Gnome - view of the image collection
 *
 * Copyright (C) 2001-2003 The Free Software Foundation
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdnd.h>

#include "eog-wrap-list.h"
#include "libeog-marshal.h"
#include "eog-image.h"
#include "eog-collection-item.h"

#define COLLECTION_DEBUG 0

#define EOG_WRAP_LIST_BORDER  12
static const int MAX_ITEM_WIDTH = EOG_COLLECTION_ITEM_MAX_WIDTH;

#define EOG_DND_URI_LIST_TYPE 	         "text/uri-list"



enum {
	EOG_DND_URI_LIST
};

static GtkTargetEntry drag_types [] = {
	{ EOG_DND_URI_LIST_TYPE,   0, EOG_DND_URI_LIST }
};


/* Used to hold signal handler IDs for models */
typedef enum {
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
	GLOBAL_WIDGET_SIZE_CHANGED,
	GLOBAL_SPACING_CHANGED,
	ITEM_SIZE_CHANGED,
	N_ITEMS_CHANGED,
	UPDATE_HINT_LAST
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
	EogImageList *model;

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

	/* Number of rows. */
	guint n_rows;

	/* Number of columns. */
	guint n_cols;

	/* Id for the idle handler.*/
	gint idle_handler_id;

	/* Update hints */
	gboolean global_update_hints [UPDATE_HINT_LAST];

	/* last id of thumbnail the user clicked */
	GnomeCanvasItem *last_item_clicked;
};

static void eog_wrap_list_class_init (EogWrapListClass *class);
static void eog_wrap_list_init (EogWrapList *wlist);
static void eog_wrap_list_dispose (GObject *object);
static void eog_wrap_list_finalize (GObject *object);

static void eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean eog_wrap_list_key_press_cb (GtkWidget *widget, GdkEventKey *event);
static void eog_wrap_list_drag_data_get_cb (GtkWidget *widget,
					    GdkDragContext *drag_context,
					    GtkSelectionData *data,
					    guint info,
					    guint time);

static void request_update (EogWrapList *wlist);
static gboolean do_update (EogWrapList *wlist);
static gint handle_canvas_click (GnomeCanvas *canvas, GdkEventButton *event, gpointer data);

G_DEFINE_TYPE (EogWrapList, eog_wrap_list, GNOME_TYPE_CANVAS)

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
	widget_class->drag_data_get = eog_wrap_list_drag_data_get_cb;

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
			      libeog_marshal_BOOLEAN__INT_POINTER,
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
			      libeog_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
}

/* object initialization function for the abstract wrapped list view */
static void
eog_wrap_list_init (EogWrapList *wlist)
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
	priv->n_rows = 0;
	priv->n_cols = 0;
	priv->last_item_clicked = NULL;
	priv->item_width = -1;
	priv->item_height = -1;

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (wlist), 
			     GDK_BUTTON1_MASK,
			     drag_types, G_N_ELEMENTS (drag_types),
			     GDK_ACTION_COPY);
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
	
	G_OBJECT_CLASS (eog_wrap_list_parent_class)->dispose (object);
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

	if (G_OBJECT_CLASS (eog_wrap_list_parent_class)->finalize)
		(* G_OBJECT_CLASS (eog_wrap_list_parent_class)->finalize) (object);
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
	guint row, col, n;
	
	priv = wlist->priv;

	col = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "column"));
	row = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "row"));

	n = col + priv->n_cols * row;

	return n;
}

static void
ensure_item_is_visible (EogWrapList *wlist, GnomeCanvasItem *item)
{
	double x1, y1, x2, y2;
	int width, height;
	int ofsx, ofsy;
	int scroll_x, scroll_y;

	width = GTK_WIDGET (wlist)->allocation.width;
	height = GTK_WIDGET (wlist)->allocation.height;
	gnome_canvas_get_scroll_offsets (GNOME_CANVAS (wlist), &ofsx, &ofsy);

	gnome_canvas_item_get_bounds (item, &x1, &y1, &x2, &y2);
	scroll_x = scroll_y = 0;
       
	if (y1 < ofsy || y2 > (ofsy + height)) {
		if (abs (y1 - ofsy) < abs (y1 - (ofsy + height))) {
			/* the item is more at the upper edge */
			scroll_y = y1 - ofsy;
		}
		else {
			/* the item is more at the lower edge */
			scroll_y = y2 - (ofsy + height);
		}
	}

	if (x1 < ofsx || x2 > (ofsx + width)) {
		if (abs (x1 - ofsx) < abs (x1 - (ofsx + width))) {
			/* the item is more at the left edge */
			scroll_x = x1 - ofsx;
		}
		else {
			/* the item is more at the right edge */
			scroll_x = x2 - (ofsx + width);
		}		
	}
		
	gnome_canvas_scroll_to (GNOME_CANVAS (wlist), 
				ofsx + scroll_x, ofsy + scroll_y); 
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
	EogImageList *model;
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
			ensure_item_is_visible (wlist, item);

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

static void
handle_item_size_changed (GnomeCanvasItem *item, EogWrapList *wlist)
{
	EogWrapListPrivate *priv;

	priv = wlist->priv;

	priv->global_update_hints [ITEM_SIZE_CHANGED] = TRUE;

	request_update (wlist);
}

/* Size_allocate handler for the abstract wrapped list view */
static void
eog_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;

	wlist = EOG_WRAP_LIST (widget);
	priv = wlist->priv;

	GTK_WIDGET_CLASS (eog_wrap_list_parent_class)->size_allocate (widget,
								    allocation);

	priv->global_update_hints[GLOBAL_WIDGET_SIZE_CHANGED] = TRUE;

	request_update (wlist);
}       

void 
eog_wrap_list_select_single (EogWrapList *wlist, EogWrapListSelectChange change)
{
	EogWrapListPrivate *priv;
	GList *node;
	GnomeCanvasItem *item;
	int i;

	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->n_selected_items != 1) return;

	node = g_list_find (priv->view_order, priv->last_item_clicked);

	if (node == NULL) return;

	switch (change) {
	case EOG_WRAP_LIST_SELECT_UP:
		for (i = 0; i < priv->n_cols && node != NULL; i++) {
			node = node->prev;
		}
		break;
	case EOG_WRAP_LIST_SELECT_DOWN:
		for (i = 0; i < priv->n_cols && node != NULL; i++) {
			node = node->next;
		}
		break;
	case EOG_WRAP_LIST_SELECT_LEFT:
		node = node->prev;
		if (node == NULL) {
			node = g_list_last (priv->view_order);
		}
		break;
	case EOG_WRAP_LIST_SELECT_RIGHT:
		node = node->next;
		if (node == NULL) {
			node = g_list_first (priv->view_order);
		}
		break;
	case EOG_WRAP_LIST_SELECT_FIRST:
		node = g_list_first (priv->view_order);
		break;
	case EOG_WRAP_LIST_SELECT_LAST:
		node = g_list_last (priv->view_order);
		break;
	default:
		g_assert_not_reached ();
	}

	if (node == NULL) return;

	item = GNOME_CANVAS_ITEM (node->data);

	set_select_status (wlist, EOG_COLLECTION_ITEM (priv->last_item_clicked), FALSE);
	set_select_status (wlist, EOG_COLLECTION_ITEM (item), TRUE);
	priv->last_item_clicked = item;

	ensure_item_is_visible (wlist, item);

	g_signal_emit (G_OBJECT (wlist), eog_wrap_list_signals [SELECTION_CHANGED], 0);
}

static gboolean 
eog_wrap_list_key_press_cb (GtkWidget *widget, GdkEventKey *event)
{
	gboolean handled = FALSE;

	switch (event->keyval) {
	case GDK_Up:
		eog_wrap_list_select_single (EOG_WRAP_LIST (widget), EOG_WRAP_LIST_SELECT_UP);
		handled = TRUE;
		break;

	case GDK_Down:
		eog_wrap_list_select_single (EOG_WRAP_LIST (widget), EOG_WRAP_LIST_SELECT_DOWN);
		handled = TRUE;
		break;

	case GDK_Left:
		eog_wrap_list_select_single (EOG_WRAP_LIST (widget), EOG_WRAP_LIST_SELECT_LEFT);
		handled = TRUE;
		break;

	case GDK_Right:
		eog_wrap_list_select_single (EOG_WRAP_LIST (widget), EOG_WRAP_LIST_SELECT_RIGHT);
		handled = TRUE;
		break;
	};

	if (!handled) 
	{
		if (GTK_WIDGET_CLASS (eog_wrap_list_parent_class)->key_press_event)
			handled = GTK_WIDGET_CLASS (eog_wrap_list_parent_class)->key_press_event (widget, event);
		else
			handled = FALSE;
	}

	return handled;
}

static void eog_wrap_list_drag_data_get_cb (GtkWidget *widget,
					    GdkDragContext *drag_context,
					    GtkSelectionData *data,
					    guint info,
					    guint time)
{
	EogWrapList *wlist;

	wlist = EOG_WRAP_LIST (widget);

	switch (info) {
	case EOG_DND_URI_LIST:
	{
		EogImage *image;
		GList *it;
		GString *str;
		GnomeVFSURI *uri;

		/* We assess that every uri has about 30 chars. This
		   is only to reduce number of reallocations when we
		   add chars to the string */
		str = g_string_sized_new (g_list_length (wlist->priv->selected_items) * 30);
		
		for (it = wlist->priv->selected_items; it != NULL; it = it->next) {
			EogCollectionItem *item = EOG_COLLECTION_ITEM (it->data);
			image = eog_collection_item_get_image (item);
			uri = eog_image_get_uri (image);

			g_string_append (str, gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE));
			g_string_append_c (str, '\n');

			g_object_unref (image);
			gnome_vfs_uri_unref (uri);
		}
		
		gtk_selection_data_set (data,
					data->target,
					8, (guchar*) str->str,
					str->len);

		g_string_free (str, TRUE);

		break;
	}

	default:
		g_assert_not_reached ();
	}
	
}


static void
add_image (EogWrapList *wlist, EogImage *image)
{
	EogWrapListPrivate *priv;
	GnomeCanvasItem *item;
	
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;
	
	item = eog_collection_item_new (gnome_canvas_root (GNOME_CANVAS (wlist)), image);
	gnome_canvas_item_move (item, -1000.0, -1000.0); /* hide item in invisible area */
	g_signal_connect (G_OBJECT (item), "event", G_CALLBACK (handle_item_event), wlist);
	g_signal_connect (G_OBJECT (item), "size_changed", G_CALLBACK (handle_item_size_changed), wlist);

	priv->view_order = g_list_prepend (priv->view_order, item);
}

static void
create_items_from_model (EogWrapList *wlist, EogImageList *model)
{
	EogWrapListPrivate *priv;
	EogIter *iter;
	gboolean success;
	EogImage *image;
	EogCollectionItem *initial_item;
	GList *it;
	int pos;
	
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));
	g_return_if_fail (EOG_IS_IMAGE_LIST (model));

	priv = wlist->priv;

	g_assert (priv->view_order == NULL);
	g_assert (priv->selected_items == NULL);
	g_assert (priv->n_selected_items == 0);

	iter = eog_image_list_get_first_iter (model);
	success = (iter != NULL);
	for (; success; success = eog_image_list_iter_next (model, iter, FALSE)) {
		image = eog_image_list_get_img_by_iter (model, iter);
		add_image (wlist, image);
		g_object_unref (image);
	}
	g_free (iter);

	priv->view_order = g_list_reverse (priv->view_order);
	priv->n_items = g_list_length (priv->view_order);

	for (it = priv->view_order; it != NULL; it = it->next) {
		eog_collection_item_load (EOG_COLLECTION_ITEM (it->data));
	}

	if (priv->view_order != NULL) {
		deselect_all (wlist);

		pos = eog_image_list_get_initial_pos (priv->model);
		if (pos == -1) return;

		initial_item = g_list_nth_data (priv->view_order, pos);

		if (set_select_status (wlist, EOG_COLLECTION_ITEM (initial_item), TRUE)) {
			priv->last_item_clicked = GNOME_CANVAS_ITEM (initial_item);
			ensure_item_is_visible (wlist, GNOME_CANVAS_ITEM (initial_item));
			g_signal_emit (G_OBJECT (wlist), eog_wrap_list_signals [SELECTION_CHANGED], 0);
		}
	}
}

/* Handler for the interval_added signal from models */
static void
model_image_added (EogImageList *model, EogImage *image, int position, gpointer data)
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

	priv->global_update_hints[GLOBAL_WIDGET_SIZE_CHANGED] = TRUE;
	request_update (wlist);
	
	eog_collection_item_load (EOG_COLLECTION_ITEM (item));
}

/* Handler for the interval_changed signal from models */
static void
model_image_removed (EogImageList *model, int pos, gpointer data)
{
	EogWrapList *wlist;
	EogWrapListPrivate *priv;
	GnomeCanvasItem *item;
	GList *node;

	g_return_if_fail (EOG_IS_WRAP_LIST (data));

	wlist = EOG_WRAP_LIST (data);
	priv = wlist->priv;

	if (pos == -1) return;

	node = g_list_nth (priv->view_order, pos);
	item = (GnomeCanvasItem*) node->data;

	/* remove item from data structures */
	if (priv->last_item_clicked == item) {
		priv->last_item_clicked = NULL;
	}
	
	set_select_status (wlist, EOG_COLLECTION_ITEM (item), FALSE);
	priv->view_order = g_list_delete_link (priv->view_order, node);

	/* delete object */
	gnome_canvas_item_hide (item);
	gnome_canvas_update_now (GNOME_CANVAS (wlist));
	gtk_object_destroy (GTK_OBJECT (item));

	/* update item arrangement */
	priv->global_update_hints[N_ITEMS_CHANGED] = TRUE;
	request_update (wlist);
}

/* Set model handler for the wrapped list view */
void
eog_wrap_list_set_model (EogWrapList *wlist, EogImageList *model)
{
	EogWrapListPrivate *priv;
	int i;
	GList *it;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->model) {
		for (i = 0; i < MODEL_SIGNAL_LAST; i++) {
			g_signal_handler_disconnect (G_OBJECT (priv->model), priv->model_ids[i]);
			priv->model_ids[i] = 0;
		}
		g_object_unref (G_OBJECT (priv->model));
	}
	priv->model = NULL;

	/* free/remove all the collection items */
	for (it = priv->view_order; it != NULL; it = it->next) {
		gtk_object_destroy (GTK_OBJECT (it->data));
	}
	g_list_free (priv->selected_items);
	priv->selected_items = NULL;
	priv->n_selected_items = 0;
	g_list_free (priv->view_order);
	priv->view_order = NULL;

	if (model) {
		priv->model = model;
		g_object_ref (G_OBJECT (model));
		
		priv->model_ids[MODEL_SIGNAL_IMAGE_ADDED] = g_signal_connect (
			G_OBJECT (model), "image-added",
			G_CALLBACK (model_image_added),
			wlist);

		priv->model_ids[MODEL_SIGNAL_IMAGE_REMOVED] = g_signal_connect (
			G_OBJECT (model), "image-removed",
			G_CALLBACK (model_image_removed),
			wlist);
		
		create_items_from_model (wlist, model);
	}

	priv->global_update_hints[N_ITEMS_CHANGED] = TRUE;
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
	
	EogWrapListPrivate *priv;

	priv = wlist->priv;

#if COLLECTION_DEBUG
	g_message ("do_layout_check called");
#endif

	if (priv->view_order == NULL)
		return FALSE;

	/* get canvas width */
	cw = GTK_WIDGET (wlist)->allocation.width;

	/* calculate new number of  columns/rows */
	n_cols_new = cw / (EOG_COLLECTION_ITEM_MAX_WIDTH + priv->col_spacing);
	
	if (cw > (n_cols_new * (EOG_COLLECTION_ITEM_MAX_WIDTH + priv->col_spacing) + EOG_COLLECTION_ITEM_MAX_WIDTH))
		n_cols_new++;
	
	if (n_cols_new == 0)
		n_cols_new = 1;
	n_rows_new = (priv->n_items + n_cols_new - 1) / n_cols_new;

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
set_item_position (EogCollectionItem *item, double x, double y)
{
	double x1, x2, y1, y2;
	double xoff, yoff;

	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (item), &x1, &y1, &x2, &y2);
	
	xoff = x - x1;
	yoff = y - y1;
		
	if (xoff || yoff)
		gnome_canvas_item_move (GNOME_CANVAS_ITEM (item), xoff, yoff);
}

static void
calculate_row_height (GList *it, int n_cols, 
		      int *image_height, int *caption_height)
{
	int i;
	EogCollectionItem *item;
	int ih, ch, w;

	*image_height = 0;
	*caption_height = 0;

	for (i = 0; (it != NULL) && (i < n_cols); i++, it = it->next) 
	{
		item = EOG_COLLECTION_ITEM (it->data);

		eog_collection_item_get_size (item, &w, &ih, &ch);

		*image_height = MAX (*image_height, ih);
		*caption_height = MAX (*caption_height, ch);
	}
}

static void
do_item_rearrangement (EogWrapList *wlist)
{
	EogWrapListPrivate *priv;
	int max_image_height;
	int max_caption_height;
	int item_width;
	int image_height;
	int caption_height;
	int row_offset;
	int n, row, col;
	double world_x, world_y;
	double sr_width, sr_height;
	EogCollectionItem *item;
	GList *it;

#if COLLECTION_DEBUG
	g_message ("do_item_rearrangement called");
#endif

	priv = wlist->priv;

	row_offset = EOG_WRAP_LIST_BORDER - priv->row_spacing;
	max_image_height = max_caption_height = 0;
	it = priv->view_order;
	col = 0;
	
	for (n = 0; it != NULL; it = it->next, n++) {
		col = n % priv->n_cols;
		row = n / priv->n_cols;
		
		if (col == 0) {
			/* the offset for the new row is the
			 * old offset plus the height of the
			 * previous row 
			 */
			row_offset = row_offset + max_image_height + max_caption_height + 
				priv->row_spacing;
			
			/* we start a new row and calculate
			 * the row height/offset for the current row
			 */
			calculate_row_height (it, priv->n_cols,
					      &max_image_height, &max_caption_height);
		}
		
		item = EOG_COLLECTION_ITEM (it->data);
		eog_collection_item_get_size (item, &item_width, &image_height, &caption_height);
		item_width = MIN (item_width, MAX_ITEM_WIDTH); /* this is only to ensure 
								  item_width <= MAX_ITEM_WIDTH */
		
		world_x = col * (MAX_ITEM_WIDTH + priv->col_spacing) + 
			(MAX_ITEM_WIDTH - item_width) / 2;
		world_y = row_offset + max_image_height - image_height;
		
		set_item_position (item, world_x, world_y);
		
		g_object_set_data (G_OBJECT (item), "row", GUINT_TO_POINTER (row));
		g_object_set_data (G_OBJECT (item), "column", GUINT_TO_POINTER (col));
	}
	
	
	/* set new canvas scroll region */
	sr_width =  priv->n_cols * (MAX_ITEM_WIDTH + priv->col_spacing) - priv->col_spacing;
	sr_height = row_offset + max_image_height + max_caption_height + EOG_WRAP_LIST_BORDER;
	
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
	if (priv->global_update_hints [N_ITEMS_CHANGED] ||
	    priv->global_update_hints [GLOBAL_SPACING_CHANGED] ||
	    priv->global_update_hints [GLOBAL_WIDGET_SIZE_CHANGED])
	{
		layout_check_needed = TRUE;
		item_rearrangement_needed = TRUE;
		
		priv->global_update_hints[GLOBAL_SPACING_CHANGED] = FALSE;
		priv->global_update_hints[GLOBAL_WIDGET_SIZE_CHANGED] = FALSE;
		priv->global_update_hints[N_ITEMS_CHANGED] = FALSE;
	} 
	else if (priv->global_update_hints [ITEM_SIZE_CHANGED]) {
		item_rearrangement_needed = TRUE;
		priv->global_update_hints [ITEM_SIZE_CHANGED] = FALSE;
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
	GList *list = NULL;
	GList *it;

	g_return_val_if_fail (EOG_IS_WRAP_LIST (wlist), NULL);

	priv = wlist->priv;

	for (it = priv->selected_items; it != NULL; it = it->next) {
		EogCollectionItem *item = EOG_COLLECTION_ITEM (it->data);
		list = g_list_prepend (list, eog_collection_item_get_image (item));
	}

	list = g_list_reverse (list);
	
	return list;
}

/* eog_wrap_list_set_current_image
 *
 * This function makes sure that the corresponding item to image is in
 * the list of selected items. Also it is marked as the last clicked
 * item. If the flag deselect_other is TRUE than all other selected
 * item will be deselected.
 */
void
eog_wrap_list_set_current_image (EogWrapList *wlist, EogImage *image, gboolean deselect_other)
{
	EogWrapListPrivate *priv;
	EogCollectionItem *item;
	gboolean selection_changed = FALSE;
	int pos;

	g_return_if_fail (EOG_IS_WRAP_LIST (wlist));
	g_return_if_fail (EOG_IS_IMAGE (image));

	priv = wlist->priv;

	/* Warning: We rely on the fact that the model list and the
	   view_order list are totally in sync wrt to the
	   sequence. AFAICS this is a valid assumption currently. */
	pos = eog_image_list_get_pos_by_img (priv->model, image); 
	if (pos == -1) return;

	item = g_list_nth_data (priv->view_order, pos);

	priv->last_item_clicked = GNOME_CANVAS_ITEM (item);

	if (deselect_other || (priv->n_selected_items == 1)) {
		deselect_all (wlist);
	}

	selection_changed = set_select_status (wlist, item, TRUE);
	if (selection_changed) {
		g_signal_emit (G_OBJECT (wlist), eog_wrap_list_signals [SELECTION_CHANGED], 0);
	}

	ensure_item_is_visible (wlist, GNOME_CANVAS_ITEM (item));
}
