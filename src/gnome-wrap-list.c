/* GNOME libraries - abstract wrapped list view
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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include "gnome-wrap-list.h"



/* Used to hold signal handler IDs for models */
typedef struct {
	guint interval_changed_id;
	guint interval_added_id;
	guint interval_removed_id;
} ModelSignalIDs;

/* Layout information for row/col major modes.  Invariants:
 *
 *   - The items_per_block field specifies the number of items that fit in the
 *     minor dimension.  If it is 0, then it means the block size has not been
 *     computed yet and as such n_display and items will be 0 and NULL,
 *     respectively.
 *
 *   - If first_index is -1, it means no items have been configured.
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

	/* Signal connection IDs for the models */
	ModelSignalIDs model_ids;
	ModelSignalIDs sel_model_ids;
	ModelSignalIDs pos_model_ids;

	/* Width and height of items */
	int item_width;
	int item_height;

	/* Spacing between rows and columns */
	int row_spacing;
	int col_spacing;

	/* Scroll adjustments */

	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	/* Real scroll offsets computed from the adjustment values */
	int h_offset;
	int v_offset;

	/* Size of scrolling region */
	int scroll_region_w;
	int scroll_region_h;

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

static void model_set (GnomeListView *view, GnomeListModel *old_model);
static void selection_model_set (GnomeListView *view, GnomeListSelectionModel *old_sel_model);
static void list_item_factory_set (GnomeListView *view, GnomeListItemFactory *old_factory);

static void set_scroll_adjustments (GnomeWrapList *wlist, GtkAdjustment *hadj, GtkAdjustment *vadj);

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

	class->set_scroll_adjustments = set_scroll_adjustments;

	widget_class->set_scroll_adjustments_signal =
		gtk_signal_new ("set_scroll_adjustments",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeWrapListClass, set_scroll_adjustments),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_ADJUSTMENT,
				GTK_TYPE_ADJUSTMENT);
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

	/* Other defaults */

	priv->shadow_type = GTK_SHADOW_NONE;

	priv->u.bm.first_index = -1;

	/* Create our canvas */

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	gtk_widget_push_composite_child ();
	priv->canvas = gnome_canvas_new ();
	gtk_widget_pop_composite_child ();

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

/* Computes the minor and major sizes for items and spaces */
static void
bm_compute_item_size (GnomeWrapList *wlist,
		      int *item_minor, int *item_major,
		      int *space_minor, int *space_major)
{
	WrapListPrivate *priv;
	int i_minor, i_major;
	int s_minor, s_major;

	priv = wlist->priv;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR) {
		i_minor = priv->item_width;
		i_major = priv->item_height;
		s_minor = priv->col_spacing;
		s_major = priv->row_spacing;
	} else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		i_minor = priv->item_height;
		i_major = priv->item_width;
		s_minor = priv->row_spacing;
		s_major = priv->col_spacing;
	} else {
		i_minor = i_major = s_minor = s_major = 0;
		g_assert_not_reached ();
	}

	if (item_minor)
		*item_minor = i_minor;

	if (item_major)
		*item_major = i_major;

	if (space_minor)
		*space_minor = s_minor;

	if (space_major)
		*space_major = s_major;
}

/* Fills a layout information structure */
static void
bm_compute_layout (GnomeWrapList *wlist, BMLayout *l)
{
	WrapListPrivate *priv;
	int border_width;
	int xthickness, ythickness;
	int width, height;

	priv = wlist->priv;

	border_width = GTK_CONTAINER (wlist)->border_width;

	if (priv->shadow_type == GTK_SHADOW_NONE)
		xthickness = ythickness = 0;
	else {
		xthickness = GTK_WIDGET (wlist)->style->klass->xthickness;
		ythickness = GTK_WIDGET (wlist)->style->klass->ythickness;
	}

	width = GTK_WIDGET (wlist)->allocation.width - 2 * (border_width + xthickness);
	height = GTK_WIDGET (wlist)->allocation.height - 2 * (border_width + ythickness);

	bm_compute_item_size (wlist,
			      &l->item_minor, &l->item_major,
			      &l->space_minor, &l->space_major);

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR) {
		l->size_minor = width;
		l->size_major = height;
		l->scroll_minor = priv->h_offset;
		l->scroll_major = priv->v_offset;
	} else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		l->size_minor = height;
		l->size_major = width;
		l->scroll_minor = priv->v_offset;
		l->scroll_major = priv->h_offset;
	} else
		g_assert_not_reached ();

	l->n_items_minor = (l->size_minor + l->space_minor) / (l->item_minor + l->space_minor);
	if (l->n_items_minor < 1)
		l->n_items_minor = 1;

	l->first_block = l->scroll_major / (l->item_major + l->space_major);
	l->last_block = (l->scroll_major + l->size_major - 1) / (l->item_major + l->space_major);
}

/* Adjusts the scrolling region of the wrap list if needed */
static void
bm_adjust_scroll_region (GnomeWrapList *wlist, BMLayout *l, int n_items)
{
	WrapListPrivate *priv;
	int minor, major;
	int w, h;

	priv = wlist->priv;

	minor = l->size_minor;
	major = ((l->item_major + l->space_major)
		 * (n_items + l->n_items_minor - 1) / l->n_items_minor
		 - l->space_major);

	if (major < l->size_major)
		major = l->size_major;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR) {
		w = minor;
		h = major;
	} else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		w = major;
		h = minor;
	} else {
		w = h = 0;
		g_assert_not_reached ();
	}

	if (w != priv->scroll_region_w || h != priv->scroll_region_h) {
		priv->scroll_region_w = w;
		priv->scroll_region_h = h;

		gnome_canvas_set_scroll_region (GNOME_CANVAS (priv->canvas), 0.0, 0.0, w - 1, h - 1);
	}
}

/* Updates the scroll offsets */
static void
bm_update_scroll (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;
	int item_major, space_major, size;

	priv = wlist->priv;

	if (!priv->need_scroll_update)
		return;

	bm_compute_item_size (wlist, NULL, &item_major, NULL, &space_major);
	size = item_major + space_major;

	/* Snap offset if necessary */

#if 0
	if ((priv->mode == GNOME_WRAP_LIST_ROW_MAJOR || priv->mode == GNOME_WRAP_LIST_COL_MAJOR)
	    && priv->use_unit_scrolling)
		val = size * (val / size);
#endif

	priv->h_offset = priv->update_scroll_h_offset;
	priv->v_offset = priv->update_scroll_v_offset;
}

/* Ensures that the item array has the correct size and layout information */
static void
bm_ensure_array_size (GnomeWrapList *wlist, BMLayout *l)
{
	WrapListPrivate *priv;
	BlockModeInfo *bm;
	int n_display;

	priv = wlist->priv;
	bm = &priv->u.bm;

	n_display = (l->last_block - l->first_block + 1) * l->n_items_minor;

	/* If the item factory changed or the array layout changed, we must
	 * discard everything.
	 */

	if (bm->items_per_block != 0 && (priv->need_factory_update
					 || bm->items_per_block != l->n_items_minor
					 || bm->n_display != n_display)) {
		int i;

		g_assert (bm->n_display > 0);
		g_assert (bm->items != NULL);

		for (i = 0; i < priv->u.bm.n_display; i++)
			if (bm->items[i])
				gtk_object_destroy (GTK_OBJECT (bm->items[i]));

		g_free (bm->items);
		bm->items = NULL;
		bm->items_per_block = 0;
		bm->first_index = -1;
		bm->n_display = 0;
	}

	priv->need_factory_update = FALSE;

	/* Allocate the item array if necessary */

	if (bm->items_per_block == 0) {
		g_assert (bm->n_display == 0);
		g_assert (bm->items == NULL);

		bm->items_per_block = l->n_items_minor;
		bm->n_display = n_display;
		bm->items = g_new0 (GnomeCanvasItem *, bm->n_display);
	}

	g_assert (bm->items_per_block == l->n_items_minor);
	g_assert (bm->n_display == n_display);
	g_assert (bm->items != NULL);
}

/* Updates a range of items, assuming the range has been clipped to the visible
 * range.
 */
static void
bm_update_range (GnomeWrapList *wlist, BMLayout *l, int update_first, int update_last)
{
	WrapListPrivate *priv;
	BlockModeInfo *bm;
	GnomeListItemFactory *factory;
	GnomeCanvasGroup *group;
	GnomeListModel *model;
	GnomeListSelectionModel *sel_model;
	int data_len;
	int i, disp_i;

	priv = wlist->priv;
	bm = &priv->u.bm;

	g_assert (update_first < update_last);
	g_assert (update_first >= bm->first_index);
	g_assert (update_last <= bm->first_index + bm->n_display);

	model = gnome_list_view_get_model (GNOME_LIST_VIEW (wlist));
	if (model)
		data_len = gnome_list_model_get_length (model);
	else
		data_len = 0;

	sel_model = gnome_list_view_get_selection_model (GNOME_LIST_VIEW (wlist));
	if (sel_model)
		g_assert (gnome_list_model_get_length (GNOME_LIST_MODEL (sel_model)) >= data_len);

	factory = gnome_list_view_get_list_item_factory (GNOME_LIST_VIEW (wlist));
	group = gnome_canvas_root (GNOME_CANVAS (priv->canvas));

	disp_i = update_first - bm->first_index;
	g_assert (disp_i >= 0 && disp_i < bm->n_display);

	for (i = update_first; i < update_last; i++, disp_i++)
		if (factory) {
			gboolean is_selected;
			gboolean is_focused;
			int pos_minor, pos_major;
			double affine[6];

			/* Create the item if needed */

			if (!bm->items[disp_i]) {
				bm->items[disp_i] = gnome_list_item_factory_create_item (
					factory, group);
				g_assert (bm->items[disp_i] != NULL);
			}

			/* Configure the item */

			if (sel_model)
				is_selected = gnome_list_selection_model_is_selected (
					sel_model, i);
			else
				is_selected = FALSE;

			is_focused = FALSE; /* FIXME */

			gnome_list_item_factory_configure_item (
				factory,
				bm->items[disp_i],
				model,
				i,
				is_selected,
				is_focused);

			/* Compute position */

			pos_minor = i % bm->items_per_block;
			pos_major = i / bm->items_per_block;

			pos_minor *= l->item_minor + l->space_minor;
			pos_major *= l->item_major + l->space_major;

			if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR)
				art_affine_translate (affine, pos_minor, pos_major);
			else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR)
				art_affine_translate (affine, pos_major, pos_minor);
			else
				g_assert_not_reached ();

			gnome_canvas_item_affine_absolute (bm->items[disp_i], affine);
		} else if (bm->items[disp_i]) {
			gtk_object_destroy (GTK_OBJECT (bm->items[disp_i]));
			bm->items[disp_i] = NULL;
		}
}

/* Updates the wrap list when in block mode */
static void
bm_update (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;
	BMLayout l;
	BlockModeInfo *bm;
	int req_update_first, req_update_last;
	int display_first, display_last;
	int update_first, update_last;
	GnomeListModel *model;
	int data_len;

	priv = wlist->priv;
	bm = &priv->u.bm;

	if (!(priv->need_factory_update || priv->need_scroll_update || priv->need_data_update))
		return;

	bm_update_scroll (wlist);
	bm_compute_layout (wlist, &l);
	bm_ensure_array_size (wlist, &l);

	/* Compute the range of items that needs to be updated */

	model = gnome_list_view_get_model (GNOME_LIST_VIEW (wlist));
	if (model)
		data_len = gnome_list_model_get_length (model);
	else
		data_len = 0;

	if (bm->first_index == -1) {
		req_update_first = 0;
		req_update_last = data_len;

		bm->first_index = l.first_block * l.n_items_minor;
	} else {
		req_update_first = priv->update_start;

		if (priv->update_n == -1)
			req_update_last = req_update_first + data_len - priv->update_start;
		else
			req_update_last = req_update_first + priv->update_n;
	}

	/* Clip update range to visible range */

	display_first = l.first_block * l.n_items_minor;
	display_last = display_first + bm->n_display;

	update_first = MAX (req_update_first, display_first);
	update_last = MIN (req_update_last, display_last);

	/* Update the items that need it */

	if (update_first < update_last)
		bm_update_range (wlist, &l, update_first, update_last);

	/* Clear items that are past the end of the data */

	if (data_len >= display_first && data_len < display_last) {
		int i;

		for (i = data_len - display_first; i < display_last; i++)
			if (bm->items[i]) {
				gtk_object_destroy (GTK_OBJECT (bm->items[i]));
				bm->items[i] = NULL;
			}
	}

	/* Done */

	bm_adjust_scroll_region (wlist, &l, data_len);
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

	wlist = GNOME_WRAP_LIST (data);
	priv = wlist->priv;

	priv->idle_id = 0;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR || priv->mode == GNOME_WRAP_LIST_COL_MAJOR)
		bm_update (wlist);
	else if (priv->mode == GNOME_WRAP_LIST_MANUAL) {
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



/* Notifications from models */

/* Handler for the interval_changed signal from models */
static void
model_interval_changed (GnomeListModel *model, guint start, guint length, gpointer data)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	int a, b;

	wlist = GNOME_WRAP_LIST (data);
	priv = wlist->priv;

	if (!priv->need_data_update) {
		priv->need_data_update = TRUE;
		priv->update_start = start;
		priv->update_n = length;
	} else {
		if (priv->update_n == -1 || length == -1) {
			if (start < priv->update_start)
				priv->update_start = start;

			priv->update_n = -1;
		} else {
			a = priv->update_start + priv->update_n;
			b = start + length;

			if (start < priv->update_start)
				priv->update_start = start;

			priv->update_n = MAX (a, b) - priv->update_start;
		}
	}

	request_update (wlist);
}

/* Handler for the interval_added signal from models */
static void
model_interval_added (GnomeListModel *model, guint start, guint length, gpointer data)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (data);
	priv = wlist->priv;

	if (!priv->need_data_update) {
		priv->need_data_update = TRUE;
		priv->update_start = start;
	} else if (start < priv->update_start)
		priv->update_start = start;

	priv->update_n = -1;
	request_update (wlist);
}

/* Handler for the interval_changed signal from models */
static void
model_interval_removed (GnomeListModel *model, guint start, guint length, gpointer data)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (data);
	priv = wlist->priv;

	if (!priv->need_data_update) {
		priv->need_data_update = TRUE;
		priv->update_start = start;
	} else if (start < priv->update_start)
		priv->update_start = start;

	priv->update_n = -1;
	request_update (wlist);
}



/* List view methods */

/* Model_set handler for the abstract wrapped list view */
static void
model_set (GnomeListView *view, GnomeListModel *old_model)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	GnomeListModel *model;

	wlist = GNOME_WRAP_LIST (view);
	priv = wlist->priv;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);

	if (old_model) {
		gtk_signal_disconnect (GTK_OBJECT (old_model), priv->model_ids.interval_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (old_model), priv->model_ids.interval_added_id);
		gtk_signal_disconnect (GTK_OBJECT (old_model), priv->model_ids.interval_removed_id);

		priv->model_ids.interval_changed_id = 0;
		priv->model_ids.interval_added_id = 0;
		priv->model_ids.interval_removed_id = 0;
	}

	model = gnome_list_view_get_model (view);
	if (model) {
		priv->model_ids.interval_changed_id = gtk_signal_connect (
			GTK_OBJECT (model), "interval_changed",
			GTK_SIGNAL_FUNC (model_interval_changed),
			view);

		priv->model_ids.interval_added_id = gtk_signal_connect (
			GTK_OBJECT (model), "interval_added",
			GTK_SIGNAL_FUNC (model_interval_added),
			view);

		priv->model_ids.interval_removed_id = gtk_signal_connect (
			GTK_OBJECT (model), "interval_removed",
			GTK_SIGNAL_FUNC (model_interval_removed),
			view);
	}
}

/* Selection_model_set handler for the abstract wrapped list view */
static void
selection_model_set (GnomeListView *view, GnomeListSelectionModel *old_sel_model)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	GnomeListSelectionModel *sel_model;

	wlist = GNOME_WRAP_LIST (view);
	priv = wlist->priv;

	priv->need_data_update = TRUE;
	priv->update_start = 0;
	priv->update_n = -1;
	request_update (wlist);

	if (old_sel_model) {
		gtk_signal_disconnect (GTK_OBJECT (old_sel_model),
				       priv->sel_model_ids.interval_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (old_sel_model),
				       priv->sel_model_ids.interval_added_id);
		gtk_signal_disconnect (GTK_OBJECT (old_sel_model),
				       priv->sel_model_ids.interval_removed_id);

		priv->sel_model_ids.interval_changed_id = 0;
		priv->sel_model_ids.interval_added_id = 0;
		priv->sel_model_ids.interval_removed_id = 0;
	}

	sel_model = gnome_list_view_get_selection_model (view);
	if (sel_model) {
		priv->sel_model_ids.interval_changed_id = gtk_signal_connect (
			GTK_OBJECT (sel_model), "interval_changed",
			GTK_SIGNAL_FUNC (model_interval_changed),
			view);

		priv->sel_model_ids.interval_added_id = gtk_signal_connect (
			GTK_OBJECT (sel_model), "interval_added",
			GTK_SIGNAL_FUNC (model_interval_added),
			view);

		priv->sel_model_ids.interval_removed_id = gtk_signal_connect (
			GTK_OBJECT (sel_model), "interval_removed",
			GTK_SIGNAL_FUNC (model_interval_removed),
			view);
	}
}

/* List_item_factory_set handler for the abstract wrapped list view */
static void
list_item_factory_set (GnomeListView *view, GnomeListItemFactory *old_factory)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;

	wlist = GNOME_WRAP_LIST (view);
	priv = wlist->priv;

	priv->need_factory_update = TRUE;
	request_update (wlist);
}



/* Wrap list methods */

/* Handles the value_changed signal from the scrolling adjustments */
static void
adjustment_value_changed (GtkAdjustment *adj, gpointer data)
{
	GnomeWrapList *wlist;
	WrapListPrivate *priv;
	int val;

	wlist = GNOME_WRAP_LIST (data);
	priv = wlist->priv;

	val = adj->value;

	if (adj == priv->hadj)
		priv->update_scroll_h_offset = val;
	else if (adj == priv->vadj)
		priv->update_scroll_v_offset = val;
	else
		g_assert_not_reached ();

	priv->need_scroll_update = TRUE;
	request_update (wlist);
}

/* Set_scroll_adjustments handler for the abstract wrapped list view.  We do the
 * standard GTK+ scrolling interface stuff here.
 */
static void
set_scroll_adjustments (GnomeWrapList *wlist, GtkAdjustment *hadj, GtkAdjustment *vadj)
{
	WrapListPrivate *priv;
	gboolean need_adjust;

	priv = wlist->priv;

	if (hadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
	else
		hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (vadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
	else
		vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	need_adjust = FALSE;

	if (priv->hadj && priv->hadj != hadj) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->hadj), wlist);
		gtk_object_unref (GTK_OBJECT (priv->hadj));
	}

	if (priv->vadj && priv->vadj != vadj) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->vadj), wlist);
		gtk_object_unref (GTK_OBJECT (priv->vadj));
	}

	if (priv->hadj != hadj) {
		priv->hadj = hadj;
		gtk_object_ref (GTK_OBJECT (priv->hadj));
		gtk_object_sink (GTK_OBJECT (priv->hadj));

		gtk_signal_connect (GTK_OBJECT (priv->hadj), "value_changed",
				    GTK_SIGNAL_FUNC (adjustment_value_changed),
				    wlist);
		need_adjust = TRUE;
	}

	if (priv->vadj != vadj) {
		priv->vadj = vadj;
		gtk_object_ref (GTK_OBJECT (priv->vadj));
		gtk_object_sink (GTK_OBJECT (priv->vadj));

		gtk_signal_connect (GTK_OBJECT (priv->vadj), "value_changed",
				    GTK_SIGNAL_FUNC (adjustment_value_changed),
				    wlist);
		need_adjust = TRUE;
	}

	if (need_adjust)
		gtk_widget_queue_resize (GTK_WIDGET (wlist));
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
	GnomePositionListModel *old_pos_model;

	g_return_if_fail (wlist != NULL);
	g_return_if_fail (GNOME_IS_WRAP_LIST (wlist));

	if (pos_model)
		g_return_if_fail (GNOME_IS_POSITION_LIST_MODEL (pos_model));

	priv = wlist->priv;

	if (pos_model == priv->pos_model)
		return;

	if (pos_model)
		gtk_object_ref (GTK_OBJECT (pos_model));

	old_pos_model = priv->pos_model;
	priv->pos_model = pos_model;

	/* FIXME: update if necessary */

	if (old_pos_model) {
		gtk_signal_disconnect (GTK_OBJECT (old_pos_model),
				       priv->pos_model_ids.interval_changed_id);
		gtk_signal_disconnect (GTK_OBJECT (old_pos_model),
				       priv->pos_model_ids.interval_added_id);
		gtk_signal_disconnect (GTK_OBJECT (old_pos_model),
				       priv->pos_model_ids.interval_removed_id);

		priv->pos_model_ids.interval_changed_id = 0;
		priv->pos_model_ids.interval_added_id = 0;
		priv->pos_model_ids.interval_removed_id = 0;

		gtk_object_unref (GTK_OBJECT (old_pos_model));
	}

	if (priv->pos_model) {
		priv->pos_model_ids.interval_changed_id = gtk_signal_connect (
			GTK_OBJECT (priv->pos_model), "interval_changed",
			GTK_SIGNAL_FUNC (model_interval_changed),
			wlist);

		priv->pos_model_ids.interval_added_id = gtk_signal_connect (
			GTK_OBJECT (priv->pos_model), "interval_added",
			GTK_SIGNAL_FUNC (model_interval_added),
			wlist);

		priv->pos_model_ids.interval_removed_id = gtk_signal_connect (
			GTK_OBJECT (priv->pos_model), "interval_removed",
			GTK_SIGNAL_FUNC (model_interval_removed),
			wlist);
	}
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
