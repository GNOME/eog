/* GNOME libraries - Icon list view
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
#include "gnome-icon-view.h"



/* Private part of the GnomeIconView structure */
struct _GnomeIconView {
	/* Data model */
	GnomeIconListModel *model;

	/* Layout mode and metrics */
	GnomeIconViewLayout layout;
	int icon_width, icon_height;
	int caption_width, caption_rows;
	int caption_spacing;
	int padding;
	int row_spacing, col_spacing;
};



static void iview_class_init (GnomeIconViewClass *class);
static void iview_init (GnomeIconView *iview);
static void iview_destroy (GtkObject *object);

static GtkWidgetClass *parent_class;



GtkType
gnome_icon_view_get_type (void)
{
	static GtkType iview_type = 0;

	if (!iview_type) {
		static const GtkTypeInfo iview_info = {
			"GnomeIconView",
			sizeof (GnomeIconView),
			sizeof (GnomeIconViewClass),
			(GtkClassInitFunc) iview_class_init,
			(GtkObjectInitfunc) iview_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		iview_type = gtk_type_unique (GTK_TYPE_WIDGET, &iview_info);
	}

	return iview_type;
}

/* Class initialization function for the icon list view */
static void
ilm_class_init (GnomeIconListModelClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WIDGET);

	object_class->destroy = ilm_destroy;
}

/* Object initialization function for the icon list view */
static void
ilm_init (GnomeIconView *iview)
{
	GnomeIconViewPrivate *priv;

	priv = g_new0 (GnomeIconViewPrivate, 1);
	iview->priv = priv;

	priv->layout = GNOME_ICON_VIEW_ROW_MAJOR;
}

/* Destroy handler for the icon list view */
static void
ilm_destroy (GtkObject *object)
{
	GnomeIconView *iview;
	GnomeIconViewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_ICON_VIEW (object));

	iview = GNOME_ICON_VIEW (object);
	priv = iview->priv;

	if (priv->model) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->model), iview);
		gtk_object_unref (GTK_OBJECT (priv->model));
		priv->model = NULL;
	}

	g_free (priv);
	iview->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/**
 * gnome_icon_view_new:
 * 
 * Creates a new icon list view widget.  You should set its data model and
 * selection model afterwards.
 * 
 * Return value: A newly-created icon list view.
 **/
GtkWidget *
gnome_icon_view_new (void)
{
	return GTK_WIDGET (gtk_type_new (GNOME_TYPE_ICON_VIEW));
}

/**
 * gnome_icon_view_set_model:
 * @iview: An icon list view widget.
 * @model: Icon list data model to use.
 * 
 * Sets the data model that an icon list view widget will use.  You should also
 * set the selection model if you want items to be selectable.
 **/
void
gnome_icon_view_set_model (GnomeIconView *iview, GnomeIconListModel *model)
{
	GnomeIconViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GNOME_IS_ICON_VIEW (view));

	if (model)
		g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));

	priv = iview->priv;

	if (model == priv->model)
		return;

	if (model)
		gtk_object_ref (GTK_OBJECT (model));

	if (priv->model) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->model), iview);
		gtk_object_unref (GTK_OBJECT (priv->model));
	}

	priv->model = model;

	if (priv->model) {
		gtk_signal_connect (GTK_OBJECT (priv->model), "interval_changed",
				    GTK_SIGNAL_FUNC (interval_changed_cb), iview);
		gtk_signal_connect (GTK_OBJECT (priv->model), "interval_added",
				    GTK_SIGNAL_FUNC (interval_changed_cb), iview);
		gtk_signal_connect (GTK_OBJECT (priv->model), "interval_removed",
				    GTK_SIGNAL_FUNC (interval_changed_cb), iview);
	}

	/* FIXME: update */
}

/**
 * gnome_icon_view_get_model:
 * @iview: An icon list view wiget.
 * 
 * Queries the data model that an icon view widget is using.
 * 
 * Return value: An icon list data model.
 **/
GnomeIconListModel *
gnome_icon_view_get_model (GnomeIconView *iview)
{
	GnomeIconViewPrivate *priv;

	g_return_val_if_fail (iview != NULL);
	g_return_val_if_fail (GNOME_IS_ICON_VIEW (iview));

	priv = iview->priv;
	return priv->model;
}

/**
 * gnome_icon_view_set_layout:
 * @iview: An icon list view widget.
 * @layout: Direction to use when laying out items.
 * @icon_width: Maximum width for icons.
 * @icon_height: Maximum height for icons.
 * @caption_width: Maximum horizontal space for captions.
 * @caption_rows: Maximum number of displayed rows for captions.
 * @caption_spacing: Space between an icon an its caption.
 * @padding: Padding around an icon and its caption.
 * @row_spacing: Spacing between rows.
 * @col_spacing: Spacing between columns.
 * 
 * Sets the layout parameters for an icon list view widget.  All the width,
 * height, and spacing parameters are in pixels.
 *
 * Supplied icons that are bigger than the specified maximum size will be scaled
 * down.  To specify that no captions be drawn, use 0 for @caption_width or
 * @caption_rows.
 **/
void
gnome_icon_view_set_layout (GnomeIconView *iview, GnomeIconViewLayout layout,
			    int icon_width, int icon_height,
			    int caption_width, int caption_rows,
			    int caption_spacing,
			    int padding,
			    int row_spacing, int col_spacing)
{
	GnomeIconViewPrivate *priv;

	g_return_if_fail (iview != NULL);
	g_return_if_fail (GNOME_IS_ICON_VIEW (iview));
	g_return_if_fail (icon_width > 0 && icon_height > 0);
	g_return_if_fail (caption_width >= 0 && caption_rows >= 0);
	g_return_if_fail (caption_spacing >= 0);
	g_return_if_fail (padding >= 0);
	g_return_if_fail (row_spacing >= 0 && col_spacing >= 0);

	priv = iview->priv;

	priv->layout = layout;
	priv->icon_width = icon_width;
	priv->icon_height = icon_height;
	priv->caption_width = caption_width;
	priv->caption_rows = caption_rows;
	priv->caption_spacing = caption_spacing;
	priv->padding = padding;
	priv->row_spacing = row_spacing;
	priv->col_spacing = col_spacing;

	/* FIXME: update */
}

/**
 * gnome_icon_view_get_layout:
 * @iview: An icon list view widget.
 * @layout: Return value for the direction to use when laying out items.
 * @icon_width: Return value for the maximum width for icons.
 * @icon_height: Return value for the maximum height for icons.
 * @caption_width: Return value for the maximum horizontal space for captions.
 * @caption_rows: Return value for the maximum number of displayed rows for captions.
 * @caption_spacing: Return value for the space between an icon an its caption.
 * @padding: Return value for the padding around an icon and its caption.
 * @row_spacing: Return value for the spacing between rows.
 * @col_spacing: Return value for the spacing between columns.
 * 
 * Queries the layout parameters of an icon list view widget.
 **/
void
gnome_icon_view_get_layout (GnomeIconView *iview, GnomeIconViewLayout *layout,
			    int *icon_width, int *icon_height,
			    int *caption_width, int *caption_rows,
			    int *caption_spacing,
			    int *padding,
			    int *row_spacing, int *col_spacing)
{
	GnomeIconViewPrivate *priv;

	g_return_if_fail (iview != NULL);
	g_return_if_fail (GNOME_IS_ICON_VIEW (iview));

	priv = iview->priv;

	if (layout)
		layout = priv->layout;

	if (icon_width)
		icon_width = priv->icon_width;

	if (icon_height)
		icon_height = priv->icon_height;

	if (caption_width)
		caption_width = priv->caption_width;

	if (caption_rows)
		caption_rows = priv->caption_rows;

	if (caption_spacing)
		caption_spacing = priv->caption_spacing;

	if (padding)
		padding = priv->padding;

	if (row_spacing)
		row_spacing = priv->row_spacing;

	if (col_spacing)
		col_spacing = priv->col_spacing;
}
