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



/* Private part of the GnomeWrapList structure */
typedef struct {
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
} WrapListPrivate;



static void gnome_wrap_list_class_init (GnomeWrapListClass *class);
static void gnome_wrap_list_init (GnomeWrapList *wlist);
static void gnome_wrap_list_destroy (GtkObject *object);

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

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (gnome_list_view_get_type ());

	object_class->destroy = gnome_wrap_list_destroy;
}

/* Object initialization function for the abstract wrapped list view */
static void
gnome_wrap_list_init (GnomeWrapList *wlist)
{
	WrapListPrivate *priv;

	priv = g_new0 (WrapListPrivate, 1);
	wlist->priv = priv;

	priv->mode = GNOME_WRAP_LIST_ROW_MAJOR;
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
