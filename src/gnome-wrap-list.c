/* GNOME libraries - abstract wrapped list view
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
#include <gtk/gtkmain.h>
#include "gnome-wrap-list.h"



/* Layout information for row/col major modes.  Invariants:
 *
 *   - The items_per_block field specifies the number of items that fit in the
 *     minor dimension.  If it is 0, then it means the block size has not been
 *     computed yet and as such n_display and items will be 0 and NULL,
 *     respectively.
 *
 *   - The items array contains as many slots as needed to fill up the wrap list
 *     at its current size.
 */
typedef struct {
	/* Number of items per row/col, 0 if not computed yet (this means items will be NULL) */
	int items_per_block;

	/* Index of first displayed item and number of displayable items */
	int first_index;
	int n_display;

	/* Array of displayable items, of length n_display */
	GnomeCanvasItem **items;
} BlockModeInfo;



/* Private part of the GnomeWrapList structure */
typedef struct {
	/* Our canvas */
	GtkWidget *canvas;

	/* Layout mode */
	GnomeWrapListMode mode;

	/* Position list model */
	GnomePositionListModel *pos_model;

	/* Width and height of items */
	int item_width;
	int item_height;

	/* Spacing between rows and columns */
	int row_spacing;
	int col_spacing;

	/* Scroll offsets */
	int h_offset;
	int v_offset;

	/* Layout information */
	union {
		BlockModeInfo bm;
	} u;

	/* Idle handler ID */
	guint idle_id;

	/* Range that needs updating, update_n is -1 if all items up to the end need updating. */
	int update_start;
	int update_n;

	/* Scroll offsets that need updating */
	int update_scroll_h_offset;
	int update_scroll_v_offset;

	/* Whether to scroll in whole rows/cols or with arbitrary offsets */
	guint use_unit_scrolling : 1;

	/* Shadow type */
	guint shadow_type : 3;

	/* Whether the item factory has changed */
	guint need_factory_update : 1;

	/* Whether some model has changed */
	guint need_data_update : 1;

	/* Whether we need to scroll */
	guint need_scroll_update : 1;
} WrapListPrivate;



static void gnome_wrap_list_class_init (GnomeWrapListClass *class);
static void gnome_wrap_list_init (GnomeWrapList *wlist);
static void gnome_wrap_list_destroy (GtkObject *object);

static void gnome_wrap_list_map (GtkWidget *widget);
static void gnome_wrap_list_unmap (GtkWidget *widget);
static void gnome_wrap_list_draw (GtkWidget *widget, GdkRectangle *area);
static void gnome_wrap_list_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gnome_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint gnome_wrap_list_expose (GtkWidget *widget, GdkEventExpose *event);

static void gnome_wrap_list_forall (GtkContainer *container, gboolean include_internals,
				    GtkCallback callback, gpointer callback_data);

static void model_set (GnomeListView *view);
static void selection_model_set (GnomeListView *view);
static void list_item_factory_set (GnomeListView *view);

static GnomeListViewClass *parent_class;



/**
 * gnome_wrap_list_get_type:
 * @void:
 *
 * Registers the #GnomeWrapList class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeWrapList class.
 **/
GtkType
gnome_wrap_list_get_type (void)
{
	static GtkType wrap_list_type = 0;

	if (!wrap_list_type) {
		static const GtkTypeInfo wrap_list_info = {
			"GnomeWrapList",
			sizeof (GnomeWrapList),
			sizeof (GnomeWrapListClass),
			(GtkClassInitFunc) gnome_wrap_list_class_init,
			(GtkObjectInitFunc) gnome_wrap_list_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		wrap_list_type = gtk_type_unique (gnome_list_view_get_type (), &wrap_list_info);
	}

	return wrap_list_type;
}

/* Class initialization function for the abstract wrapped list view */
static void
gnome_wrap_list_class_init (GnomeWrapListClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;
	GnomeListViewClass *list_view_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	container_class = (GtkContainerClass *) class;
	list_view_class = (GnomeListViewClass *) class;

	parent_class = gtk_type_class (gnome_list_view_get_type ());

	object_class->destroy = gnome_wrap_list_destroy;

	widget_class->map = gnome_wrap_list_map;
	widget_class->unmap = gnome_wrap_list_unmap;
	widget_class->draw = gnome_wrap_list_draw;
	widget_class->size_request = gnome_wrap_list_size_request;
	widget_class->size_allocate = gnome_wrap_list_size_allocate;
	widget_class->expose_event = gnome_wrap_list_expose;

	container_class->forall = gnome_wrap_list_forall;

	list_view_class->model_set = model_set;
	list_view_class->selection_model_set = selection_model_set;
	list_view_class->list_item_factory_set = list_item_factory_set;
}

/* object initialization function for the abstract wrapped list view */
static void
gnome_wrap_list_init (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	priv = g_new0 (WrapListPrivate, 1);
	wlist->priv = priv;

	GTK_WIDGET_SET_FLAGS (wlist, GTK_NO_WINDOW);

	priv->mode = GNOME_WRAP_LIST_ROW_MAJOR;

	/* Semi-sane default values */

	priv->item_width = 1;
	priv->item_height = 1;

	priv->shadow_type = GTK_SHADOW_NONE;

	/* Create our canvas */

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	gtk_widget_push_composite_child ();
	priv->canvas = gnome_canvas_new_aa ();
	gtk_widget_pop_composite_child ();

	gnome_canvas_set_scroll_region (GNOME_CANVAS (priv->canvas), 0, 0, 10000, 10000);

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_widget_set_parent (priv->canvas, GTK_WIDGET (wlist));
	gtk_widget_show (priv->canvas);
}

/* Destroy handler for the abstract wrapped list view */
static void
gnome_wrap_list_destroy (GtkObject *object)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (object));

	wlist = GNOME_WRAP_LIST (object);
	priv = wlist->priv;

	if (priv->pos_model)
		gtk_object_unref (GTK_OBJECT (priv->pos_model));

	/* FIXME: free the items and item array */

	gtk_widget_unparent (priv->canvas);
	priv->canvas = NULL;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Row/col (block mode) functions */

/* Describes layout information */
typedef struct {
	/* Size of items */
	int item_minor, item_major;

	/* Spacing between items */
	int space_minor, space_major;

	/* Canvas size */
	int size_minor, size_major;

	/* Scrolling offsets */
	int scroll_minor, scroll_major;

	/* Number of items that fit in the minor dimension */
	int n_items_minor;

	/* First and last blocks that fit */
	int first_block, last_block;
} BMLayout;

/* Fills a layout information structure */
static void
bm_compute_layout (GnomeWrapList *wlist, BMLayout *l)
{
	WrapListPrivate *priv;

	priv = wlist->priv;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR) {
		l->item_minor = priv->item_width;
		l->item_major = priv->item_height;
		l->space_minor = priv->col_spacing;
		l->space_major = priv->row_spacing;
		l->size_minor = GTK_WIDGET (wlist)->allocation.width;
		l->size_major = GTK_WIDGET (wlist)->allocation.height;
		l->scroll_minor = priv->h_offset;
		l->scroll_major = priv->v_offset;
	} else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		l->item_minor = priv->item_height;
		l->item_major = priv->item_width;
		l->space_minor = priv->row_spacing;
		l->space_major = priv->col_spacing;
		l->size_minor = GTK_WIDGET (wlist)->allocation.height;
		l->size_major = GTK_WIDGET (wlist)->allocation.width;
		l->scroll_minor = priv->v_offset;
		l->scroll_major = priv->h_offset;
	} else
		g_assert_not_reached ();

	l->n_items_minor = (l->size_minor - l->space_minor) / (l->item_minor + l->space_minor);
	if (l->n_items_minor < 1)
		l->n_items_minor = 1;

	l->first_block = l->scroll_major / (l->item_major + l->space_major);
	l->last_block = (l->scroll_major + l->size_major - 1) / (l->item_major + l->space_major);
}

/* Updates the wrap list when a data model has changed */
static void
bm_update_data (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;
	BMLayout l;
	int n_display;
	int i;
	int update_first, update_index;
	int update_n;
	GnomeListModel *model;
	int data_len;
	GnomeListSelectionModel *sel_model;
	GnomeListItemFactory *factory;
	GnomeCanvasGroup *group;

	priv = wlist->priv;

	if (!(priv->need_factory_update || priv->need_data_update))
		return;

	g_print ("really updating!\n");

	bm_compute_layout (wlist, &l);

	/* If the item factory or the number of items per block changed, we need
         * to discard all items.
	 */

	if (priv->u.bm.items_per_block != 0
	    && (priv->need_factory_update || priv->u.bm.items_per_block != l.n_items_minor)) {
		g_assert (priv->u.bm.n_display > 0);
		g_assert (priv->u.bm.items != NULL);

		for (i = 0; i < priv->u.bm.n_display; i++)
			if (priv->u.bm.items[i]) {
				gtk_object_destroy (GTK_OBJECT (priv->u.bm.items[i]));
				priv->u.bm.items[i] = NULL;
			}

		priv->u.bm.first_index = -1;
	}

	priv->need_factory_update = FALSE;

	/* Resize the item array if needed */

	n_display = (l.last_block - l.first_block + 1) * l.n_items_minor;

	if (priv->u.bm.items_per_block == 0) {
		g_assert (priv->u.bm.n_display == 0);
		g_assert (priv->u.bm.items == NULL);

		priv->u.bm.items = g_new0 (GnomeCanvasItem *, n_display);
		priv->u.bm.first_index = -1;
	} else {
		g_assert (priv->u.bm.n_display > 0);
		g_assert (priv->u.bm.items != NULL);

		if (priv->u.bm.n_display > n_display) {
			for (i = n_display; i < priv->u.bm.n_display; i++)
				if (priv->u.bm.items[i])
					gtk_object_destroy (GTK_OBJECT (priv->u.bm.items[i]));

			priv->u.bm.items = g_renew (GnomeCanvasItem *, priv->u.bm.items, n_display);
		} else if (priv->u.bm.n_display < n_display) {
			priv->u.bm.items = g_renew (GnomeCanvasItem *, priv->u.bm.items, n_display);

			for (i = priv->u.bm.n_display; i < n_display; i++)
				priv->u.bm.items[i] = NULL;
		}
	}

	priv->u.bm.items_per_block = l.n_items_minor;
	priv->u.bm.n_display = n_display;

	/* Compute the range of items that needs to be updated */

	model = gnome_list_view_get_model (GNOME_LIST_VIEW (wlist));

	if (model)
		data_len = gnome_list_model_get_length (model);
	else
		data_len = 0;

	if (priv->update_n == -1)
		update_n = data_len - priv->update_start;
	else
		update_n = priv->update_n;

	if (priv->u.bm.first_index == -1) {
		update_first = l.first_block * l.n_items_minor;
		update_index = 0;
		update_n = MIN (update_n, n_display);
		priv->u.bm.first_index = update_first;
	} else {
		update_first = priv->update_start;
		update_index = priv->update_start - priv->u.bm.first_index;
		update_n = MIN (update_n, n_display) - update_index;
	}

	g_assert (update_n >= 0 && update_n <= n_display);

	/* Update the items that need it */

	factory = gnome_list_view_get_list_item_factory (GNOME_LIST_VIEW (wlist));
	group = gnome_canvas_root (GNOME_CANVAS (priv->canvas));

	sel_model = gnome_list_view_get_selection_model (GNOME_LIST_VIEW (wlist));
	if (sel_model)
		g_assert (gnome_list_model_get_length (GNOME_LIST_MODEL (sel_model)) >= data_len);

	for (i = 0; i < update_n; i++) {
		if (factory) {
			gboolean is_selected;
			gboolean is_focused;
			int pos_minor, pos_major;
			double affine[6];

			if (!priv->u.bm.items[update_index + i])
				priv->u.bm.items[update_index + i] =
					gnome_list_item_factory_create_item (factory, group);

			if (sel_model)
				is_selected = gnome_list_selection_model_is_selected (
					sel_model, update_first + i);
			else
				is_selected = FALSE;

			is_focused = FALSE; /* FIXME */

			gnome_list_item_factory_configure_item (
				factory,
				priv->u.bm.items[update_index + i],
				model,
				update_first + i,
				is_selected,
				is_focused);

			pos_minor = (update_first + i) % priv->u.bm.items_per_block;
			pos_major = (update_first + i) / priv->u.bm.items_per_block;

			pos_minor *= l.item_minor + l.space_minor;
			pos_major *= l.item_major + l.space_major;

			if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR)
				art_affine_translate (affine, pos_minor, pos_major);
			else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR)
				art_affine_translate (affine, pos_major, pos_minor);
			else
				g_assert_not_reached ();

			g_print ("putting item %d at coords (%d, %d)\n",
				 update_first + i,
				 pos_minor, pos_major);

			gnome_canvas_item_affine_absolute (priv->u.bm.items[update_index + i],
							   affine);
		} else if (priv->u.bm.items[update_index + i]) {
			gtk_object_destroy (GTK_OBJECT (priv->u.bm.items[update_index + i]));
			priv->u.bm.items[update_index + i] = NULL;
		}
	}

	/* Clear items that are past the end of the data */

	if (update_first + update_n >= data_len)
		for (; i < n_display - update_n; i++)
			if (priv->u.bm.items[update_index + i]) {
				gtk_object_destroy (GTK_OBJECT (priv->u.bm.items[update_index + i]));
				priv->u.bm.items[update_index + i] = NULL;
			}

	/* Done */

	priv->need_data_update = FALSE;
}



/* Delayed update functions */

/* Performs the delayed update operations as an idle handler */
static gint
update (gpointer data)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	GDK_THREADS_ENTER ();

	g_print ("updating!\n");

	wlist = GNOME_WRAP_LIST (data);
	priv = wlist->priv;

	priv->idle_id = 0;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR || priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		/* FIXME: update scroll as well */
		bm_update_data (wlist);
	} else if (priv->mode == GNOME_WRAP_LIST_MANUAL) {
		/* FIXME */
	} else
		g_assert_not_reached ();

	GDK_THREADS_LEAVE ();

	return FALSE;
}

/* Requests that the wrapped list view eventually be updated */
static void
request_update (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	priv = wlist->priv;

	if (priv->idle_id != 0)
		return;

	priv->idle_id = gtk_idle_add_priority (G_PRIORITY_HIGH_IDLE, update, wlist);
}



/* Widget methods */

/* Map handler for the abstract wrapped list view */
static void
gnome_wrap_list_map (GtkWidget *widget)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (widget);
	priv = wlist->priv;

	if (GTK_WIDGET_MAPPED (widget))
		return;

	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

	if (!GTK_WIDGET_MAPPED (priv->canvas))
		gtk_widget_map (priv->canvas);
}

/* Unmap handler for the abstract wrapped list view */
static void
gnome_wrap_list_unmap (GtkWidget *widget)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (widget);
	priv = wlist->priv;

	if (!GTK_WIDGET_MAPPED (widget))
		return;

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
	gtk_widget_queue_clear (widget);

	if (GTK_WIDGET_MAPPED (priv->canvas))
		gtk_widget_unmap (priv->canvas);
}

/* Draws the shadow around the wrapped list */
static void
draw_shadow (GnomeWrapList *wlist, GdkRectangle *area)
{
	WrapListPrivate *priv;
	GtkWidget *widget;
	int border_width;

	priv = wlist->priv;
	widget = GTK_WIDGET (wlist);

	border_width = GTK_CONTAINER (wlist)->border_width;

	gtk_paint_shadow (widget->style,
			  widget->window,
			  GTK_STATE_NORMAL, priv->shadow_type,
			  area, widget,
			  NULL,
			  widget->allocation.x + border_width,
			  widget->allocation.y + border_width,
			  widget->allocation.width - 2 * border_width,
			  widget->allocation.height - 2 * border_width);
}

/* Draw handler for the abstract wrapped list view */
static void
gnome_wrap_list_draw (GtkWidget *widget, GdkRectangle *area)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	GdkRectangle canvas_area;

	wlist = GNOME_WRAP_LIST (widget);
	priv = wlist->priv;

	if (GTK_WIDGET_DRAWABLE (widget))
		draw_shadow (wlist, area);

	if (gtk_widget_intersect (priv->canvas, area, &canvas_area))
		gtk_widget_draw (priv->canvas, &canvas_area);
}

/* Size_request handler for the abstract wrapped list view */
static void
gnome_wrap_list_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	int border_width;

	wlist = GNOME_WRAP_LIST (widget);
	priv = wlist->priv;

	gtk_widget_size_request (priv->canvas, requisition);

	border_width = GTK_CONTAINER (widget)->border_width;

	requisition->width += 2 * border_width;
	requisition->height += 2 * border_width;

	if (priv->shadow_type != GTK_SHADOW_NONE) {
		requisition->width += 2 * widget->style->klass->xthickness;
		requisition->height += 2 * widget->style->klass->ythickness;
	}
}

/* Size_allocate handler for the abstract wrapped list view */
static void
gnome_wrap_list_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	int border_width;
	int xthickness, ythickness;
	GtkAllocation canvas_alloc;

	wlist = GNOME_WRAP_LIST (widget);
	priv = wlist->priv;

	widget->allocation = *allocation;

	border_width = GTK_CONTAINER (widget)->border_width;

	if (priv->shadow_type == GTK_SHADOW_NONE)
		xthickness = ythickness = 0;
	else {
		xthickness = widget->style->klass->xthickness;
		ythickness = widget->style->klass->ythickness;
	}

	canvas_alloc.x = allocation->x + border_width + xthickness;
	canvas_alloc.y = allocation->y + border_width + ythickness;
	canvas_alloc.width = allocation->width - 2 * (border_width + xthickness);
	canvas_alloc.height = allocation->height - 2 * (border_width + ythickness);

	gtk_widget_size_allocate (priv->canvas, &canvas_alloc);

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);
}

/* Expose handler for the abstract wrapped list view */
static gint
gnome_wrap_list_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GnomeWrapList *wlist;

	wlist = GNOME_WRAP_LIST (widget);

	if (GTK_WIDGET_DRAWABLE (widget))
		draw_shadow (wlist, &event->area);

	return FALSE;
}



/* Container methods */

/* Forall handler for the abstract wrapped list view */
static void
gnome_wrap_list_forall (GtkContainer *container, gboolean include_internals,
			GtkCallback callback, gpointer callback_data)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (container);
	priv = wlist->priv;

	if (!include_internals)
		return;

	(* callback) (priv->canvas, callback_data);
}



/* List view methods */

/* Model_set handler for the abstract wrapped list view */
static void
model_set (GnomeListView *view)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (view);
	priv = wlist->priv;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);
}

/* Selection_model_set handler for the abstract wrapped list view */
static void
selection_model_set (GnomeListView *view)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (view);
	priv = wlist->priv;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);
}

/* List_item_factory_set handler for the abstract wrapped list view */
static void
list_item_factory_set (GnomeListView *view)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (view);
	priv = wlist->priv;

	priv->need_factory_update = TRUE;
	request_update (wlist);
}



/* Exported functions */

/**
 * gnome_wrap_list_set_mode:
 * @wlist: A wrapped list view.
 * @mode: Desired layout mode.
 *
 * Sets the layout mode of a wrapped list view.
 **/
void
gnome_wrap_list_set_mode (GnomeWrapList *wlist, GnomeWrapListMode mode)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));

	priv = wlist->priv;

	if (priv->mode == mode)
		return;

	/* FIXME: implement this */

	priv->mode = mode;
}

/**
 * gnome_wrap_list_get_mode:
 * @wlist: A wrapped list view.
 *
 * Queries the current layout mode of a wrapped list view.
 *
 * Return value: The layout mode of the specified wrapped list view.
 **/
GnomeWrapListMode
gnome_wrap_list_get_mode (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	g_return_val_if_fail (wlist != NULL, GNOME_WRAP_LIST_ROW_MAJOR);
	g_return_val_if_fail (GNOME_IS_WRAP_LIST (wlist), GNOME_WRAP_LIST_ROW_MAJOR);

	priv = wlist->priv;
	return priv->mode;
}

/**
 * gnome_wrap_list_set_position_model:
 * @wlist: A wrapped list view.
 * @pos_model: A position list model.
 *
 * sets the position list model for a wrapped list view.
 **/
void
gnome_wrap_list_set_position_model (GnomeWrapList *wlist, GnomePositionListModel *pos_model)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));

	if (pos_model)
		g_return_if_fail (GNOME_IS_POSITION_LIST_MODEL (pos_model));

	priv = wlist->priv;

	if (pos_model == priv->pos_model)
		return;

	if (pos_model)
		gtk_object_ref (GTK_OBJECT (pos_model));

	if (priv->pos_model)
		gtk_object_unref (GTK_OBJECT (priv->pos_model));

	priv->pos_model = pos_model;

	/* FIXME: update if necessary */
}

/**
 * gnome_wrap_list_get_position_model:
 * @wlist: A wrapped list view.
 *
 * Queries the position list model that a wrapped list view is using.
 *
 * Return value: The position list model.
 **/
GnomePositionListModel *
gnome_wrap_list_get_position_model (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	g_return_val_if_fail (wlist != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_WRAP_LIST (wlist), NULL);

	priv = wlist->priv;
	return priv->pos_model;
}

/**
 * gnome_wrap_list_set_item_size:
 * @wlist: A wrapped list view.
 * @width: Width of items in pixels.
 * @height: Height of items in pixels.
 *
 * Sets the size of items in a wrapped list view.  All items will have the same
 * base size.
 **/
void
gnome_wrap_list_set_item_size (GnomeWrapList *wlist, int width, int height)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);

	priv = wlist->priv;
	priv->item_width = width;
	priv->item_height = height;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);
}

/**
 * gnome_wrap_list_get_item_size:
 * @wlist: A wrapped list view.
 * @width: Return value for the width of items.
 * @height: Return value for the height of items.
 *
 * Queries the size of items in a wrapped list view.
 **/
void
gnome_wrap_list_get_item_size (GnomeWrapList *wlist, int *width, int *height)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));
	g_return_if_fail (width > 0);
	g_return_if_fail (height > 0);

	priv = wlist->priv;

	if (width)
		*width = priv->item_width;

	if (height)
		*height = priv->item_height;
}

/**
 * gnome_wrap_list_set_row_spacing:
 * @wlist: A wrapped list view.
 * @spacing: Spacing between rows in pixels.
 *
 * Sets the spacing between the rows of a wrapped list view.
 **/
void
gnome_wrap_list_set_row_spacing (GnomeWrapList *wlist, int spacing)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));
	g_return_if_fail (spacing >= 0);

	priv = wlist->priv;
	priv->row_spacing = spacing;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);
}

/**
 * gnome_wrap_list_get_row_spacing:
 * @wlist: A wrapped list view.
 *
 * Queries the spacing between rows of a wrapped list view.
 *
 * Return value: Spacing between rows in pixels.
 **/
int
gnome_wrap_list_get_row_spacing (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	g_return_val_if_fail (wlist != NULL, -1);
	g_return_val_if_fail (GNOME_IS_WRAP_LIST (wlist), -1);

	priv = wlist->priv;
	return priv->row_spacing;
}

/**
 * gnome_wrap_list_set_col_spacing:
 * @wlist: A wrapped list view.
 * @spacing: Spacing between columns in pixels.
 *
 * Sets the spacing between the columns of a wrapped list view.
 **/
void
gnome_wrap_list_set_col_spacing (GnomeWrapList *wlist, int spacing)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));
	g_return_if_fail (spacing >= 0);

	priv = wlist->priv;
	priv->col_spacing = spacing;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);
}

/**
 * gnome_wrap_list_get_col_spacing:
 * @wlist: A wrapped list view.
 *
 * Queries the spacing between columns of a wrapped list view.
 *
 * Return value: Spacing between columns in pixels.
 **/
int
gnome_wrap_list_get_col_spacing (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	g_return_val_if_fail (wlist != NULL, -1);
	g_return_val_if_fail (GNOME_IS_WRAP_LIST (wlist), -1);

	priv = wlist->priv;
	return priv->col_spacing;
}

/**
 * gnome_wrap_list_set_use_unit_scrolling:
 * @wlist: A wrapped list view.
 * @use_unit_scrolling: TRUE for scrolling by whole rows or columns, FALSE, otherwise.
 *
 * Sets whether a wrapped list view should scroll in whole row or column
 * increments, or whether arbitrary scroll offsets should be allowed.
 **/
void
gnome_wrap_list_set_use_unit_scrolling (GnomeWrapList *wlist, gboolean use_unit_scrolling)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));

	priv = wlist->priv;
	priv->use_unit_scrolling = use_unit_scrolling ? TRUE : FALSE;

	/* FIXME: scroll a bit if necessary */
}

/**
 * gnome_wrap_list_get_use_unit_scrolling:
 * @wlist: A wrapped list view.
 *
 * Queries whether a wrapped list view scrolls by whole rows or columns.
 *
 * Return value: TRUE if it scrolls by whole rows or columns, or FALSE if
 * arbitrary scroll offsets are allowed.
 **/
gboolean
gnome_wrap_list_get_use_unit_scrolling (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	g_return_val_if_fail (wlist != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_WRAP_LIST (wlist), FALSE);

	priv = wlist->priv;
	return priv->use_unit_scrolling;
}

/**
 * gnome_wrap_list_set_shadow_type:
 * @wlist: A wrapped list view.
 * @shadow_type: A shadow type.
 *
 * Sets the shadow type of a wrapped list view.  This is the shadow frame that
 * is drawn around the list contents.
 **/
void
gnome_wrap_list_set_shadow_type (GnomeWrapList *wlist, GtkShadowType shadow_type)
{
	WrapListPrivate *priv;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));
	g_return_if_fail (shadow_type >= GTK_SHADOW_NONE && shadow_type <= GTK_SHADOW_ETCHED_OUT);

	priv = wlist->priv;

	if (priv->shadow_type == shadow_type)
		return;

	priv->shadow_type = shadow_type;
	gtk_widget_queue_resize (GTK_WIDGET (wlist));
}

/**
 * gnome_wrap_list_get_shadow_type:
 * @wlist: A wrapped list view.
 *
 * Queries the shadow type of a wrapped list view.
 *
 * Return value: The wrapped list view's current shadow type.
 **/
GtkShadowType
gnome_wrap_list_get_shadow_type (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	g_return_val_if_fail (wlist != NULL, GTK_SHADOW_NONE);
	g_return_val_if_fail (GNOME_IS_WRAP_LIST (wlist), GTK_SHADOW_NONE);

	priv = wlist->priv;
	return priv->shadow_type;
}
