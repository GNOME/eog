/* GNOME libraries - icon view for an icon list model
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



/**
 * gnome_icon_view_get_type:
 * @void:
 *
 * Registers the #GnomeIconView class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeIconView class.
 **/
GtkType
gnome_icon_view_get_type (void)
{
	static GtkType icon_view_type = 0;

	if (!icon_view_type) {
		static const GtkTypeInfo icon_view_info = {
			"GnomeIconView",
			sizeof (GnomeIconView),
			sizeof (GnomeIconViewClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		icon_view_type = gtk_type_unique (gnome_wrap_list_get_type (), &icon_view_info);
	}

	return icon_view_type;
}



/* Exported functions */

/**
 * gnome_icon_view_new:
 * @void:
 *
 * Creates a new icon view widget.  You should set its data, selection, and
 * position models afterwards with gnome_icon_view_set_model(),
 * gnome_list_view_set_selection_model(), and
 * gnome_wrap_list_set_position_model(), respectively.
 *
 * Return value: A newly created icon view widget.
 **/
GtkWidget *
gnome_icon_view_new (void)
{
	return GTK_WIDGET (gtk_type_new (GNOME_TYPE_ICON_VIEW));
}

/**
 * gnome_icon_view_set_model:
 * @iview: An icon view.
 * @model: Icon list model to use.
 *
 * Sets the data model that an icon view widget will use.
 **/
void
gnome_icon_view_set_model (GnomeIconView *iview, GnomeIconListModel *model)
{
	g_return_if_fail (iview != NULL);
	g_return_if_fail (GNOME_IS_ICON_VIEW (iview));

	if (model) {
		g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));
		gnome_list_view_set_model (GNOME_LIST_VIEW (iview), GNOME_LIST_MODEL (model));
	} else
		gnome_list_view_set_model (GNOME_LIST_VIEW (iview), NULL);
}
