/* GNOME libraries - abstract wrapped list view
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gnome-wrap-list.h"



/* Private part of the GnomeWrapList structure */
typedef struct {
	/* Layout mode */
	GnomeWrapListMode mode;
} WrapListPrivate;



static void gnome_wrap_list_class_init (GnomeWrapListClass *class);
static void gnome_wrap_list_init (GnomeWrapList *wlist);
static void gnome_wrap_list_destroy (GtkObject *object);

static GnomeListViewClass *parent_class;



/**
 * gnome_wrap_list_get_type:
 * @void:
 *
 * Registers the &GnomeWrapList class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GnomeWrapList class.
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
 * Sets the layout mode of a wrapped list view widget.
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
 * Queries the current layout mode of a wrapped list view widget.
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
