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
#include "gnome-wrap-list.h"



/* Layout information for row/col major modes */
typedef struct {
	/* Number of icons per row/col, 0 if not computed yet */
	int icons_per_block;

	/* Index of first displayed icon and number of displayed icons */
	int first_index;
	int n_displayed;

	/* Array of displayed icons */
	GnomeCanvasItem **icons;
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

	/* Whether to scroll in whole rows/cols or with arbitrary offsets */
	guint use_unit_scrolling : 1;
} WrapListPrivate;



static void gnome_wrap_list_class_init (GnomeWrapListClass *class);
static void gnome_wrap_list_init (GnomeWrapList *wlist);
static void gnome_wrap_list_destroy (GtkObject *object);

static void gnome_wrap_list_forall (GtkContainer *container, gboolean include_internals,
				    GtkCallback callback, gpointer callback_data);

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
	GtkContainerClass *container_class;

	object_class = (GtkObjectClass *) class;
	container_class = (GtkContainerClass *) class;

	parent_class = gtk_type_class (gnome_list_view_get_type ());

	object_class->destroy = gnome_wrap_list_destroy;

	container_class->forall = gnome_wrap_list_forall;
}

/* Object initialization function for the abstract wrapped list view */
static void
gnome_wrap_list_init (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	priv = g_new0 (WrapListPrivate, 1);
	wlist->priv = priv;

	priv->mode = GNOME_WRAP_LIST_ROW_MAJOR;

	/* Create our canvas */

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	priv->canvas = gnome_canvas_new ();

	gtk_widget_pop_colormap ();
	gtk_widget_pop_visual ();

	gtk_widget_set_parent (priv->canvas, GTK_WIDGET (wlist));
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

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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



/* Row/col (block mode) functions */

/* Returns the major and minor layout dimensions based on the orientation of the
 * block mode.
 */
static void
bm_get_dimensions (GnomeWrapList *wlist,
		   int *icon_minor, int *icon_major,
		   int *space_minor, int *space_major,
		   int *size_minor, int *size_major)
{
	WrapListPrivate *priv;

	priv = wlist->priv;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR) {
		*icon_minor = priv->icon_width;
		*icon_major = priv->icon_height;
		*space_minor = priv->col_spacing;
		*space_major = priv->row_spacing;
		*size_minor = GTK_WIDGET (wlist)->allocation.width;
		*size_major = GTK_WIDGET (wlist)->allocation.height;
	} else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		*icon_minor = priv->icon_height;
		*icon_major = priv->icon_width;
		*space_minor = priv->row_spacing;
		*space_major = priv->col_spacing;
		*size_minor = GTK_WIDGET (wlist)->allocation.height;
		*size_major = GTK_WIDGET (wlist)->allocation.width;
	} else
		g_assert_not_reached ();
}

/* Returns the major and minor scroll offsets */
static void
bm_get_scroll_offsets (GnomeWrapList *wlist, int *scroll_major, int *scroll_minor)
{
	WrapListPrivate *priv;

	priv = wlist->priv;

	if (priv->mode == GNOME_WRAP_LIST_ROW_MAJOR) {
		*scroll_minor = priv->h_offset;
		*scroll_major = priv->v_offset;
	} else if (priv->mode == GNOME_WRAP_LIST_COL_MAJOR) {
		*scroll_minor = priv->v_offset;
		*scroll_major = priv->h_offset;
	} else
		g_assert_not_reached ();
}

/* Computes the range of icons that can be displayed based on the current size
 * and scroll offset.
 */
static void
bm_compute_display_range (GnomeWrapList *wlist, int *first, int *n)
{
	WrapListPrivate *priv;
	int icon_minor, icon_major;
	int space_minor, space_major;
	int size_minor, size_major;
	int scroll_minor, scroll_major;
	int n_minor, n_major;
	int first_block, last_block;

	priv = wlist->priv;

	bm_get_dimensions (wlist,
			   &icon_minor, &icon_major,
			   &space_minor, &space_major,
			   &size_minor, &size_major);

	bm_get_scroll_offsets (wlist, &scroll_minor, &scroll_major);

	n_minor = (size_minor - space_minor) / (icon_minor + space_minor);
	if (n_minor <= 0)
		n_minor = 1;

	first_block = scroll_major / (icon_major + space_major);
	last_block = (scroll_major + size_major - 1) / (icon_major + space_major);

	*first = first_block * n_minor;

	n_major = last_block - first_block + 1;
	*n = n_major * n_minor;
}

/* Updates a range of icons */
static void
bm_update_range (GnomeWrapList *wlist, int first, int n_items)
{
	int disp_first, disp_n;

	bm_compute_display_range (wlist, &disp_first, &disp_n);
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

	/* FIXME: layout */
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

	/* FIXME: layout */
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

	/* FIXME: layout */
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
