/* GNOME libraries - abstract list item factory
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
#include <gtk/gtksignal.h>
#include "gnome-list-item-factory.h"



/* Signal IDs */
enum {
	CREATE_ITEM,
	CONFIGURE_ITEM,
	GET_ITEM_SIZE,
	LAST_SIGNAL
};

static void gnome_list_item_factory_class_init (GnomeListItemFactoryClass *class);

static void marshal_create_item (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);
static void marshal_configure_item (GtkObject *object, GtkSignalFunc func, gpointer data,
				    GtkArg *args);
static void marshal_get_item_size (GtkObject *object, GtkSignalFunc func, gpointer data,
				   GtkArg *args);

static guint li_factory_signals[LAST_SIGNAL];



/**
 * gnome_list_item_factory_get_type:
 * @void:
 *
 * Registers the #GnomeListItemFactory class if necessary, and returns the type
 * ID associated to it.
 *
 * Return value: The type ID of the #GnomeListItemFactory class.
 **/
GtkType
gnome_list_item_factory_get_type (void)
{
	static GtkType li_factory_type = 0;

	if (!li_factory_type) {
		static const GtkTypeInfo li_factory_info = {
			"GnomeListItemFactory",
			sizeof (GnomeListItemFactory),
			sizeof (GnomeListItemFactoryClass),
			(GtkClassInitFunc) gnome_list_item_factory_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		li_factory_type = gtk_type_unique (gtk_object_get_type (), &li_factory_info);
	}

	return li_factory_type;
}

/* Class initialization function for the abstract list item factory */
static void
gnome_list_item_factory_class_init (GnomeListItemFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	li_factory_signals[CREATE_ITEM] =
		gtk_signal_new ("create_item",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListItemFactoryClass, create_item),
				marshal_create_item,
				GNOME_TYPE_CANVAS_ITEM, 1,
				GNOME_TYPE_CANVAS_GROUP);
	li_factory_signals[CONFIGURE_ITEM] =
		gtk_signal_new ("configure_item",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListItemFactoryClass, configure_item),
				marshal_configure_item,
				GTK_TYPE_NONE, 5,
				GNOME_TYPE_CANVAS_ITEM,
				GNOME_TYPE_LIST_MODEL,
				GTK_TYPE_UINT,
				GTK_TYPE_BOOL,
				GTK_TYPE_BOOL);
	li_factory_signals[GET_ITEM_SIZE] =
		gtk_signal_new ("get_item_size",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeListItemFactoryClass, get_item_size),
				marshal_get_item_size,
				GTK_TYPE_NONE, 5,
				GNOME_TYPE_CANVAS_ITEM,
				GNOME_TYPE_LIST_MODEL,
				GTK_TYPE_UINT,
				GTK_TYPE_POINTER,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, li_factory_signals, LAST_SIGNAL);
}



/* Marshalers */

typedef GnomeCanvasItem *(* CreateItemFunc) (GtkObject *object, GtkObject *parent, gpointer data);

static void
marshal_create_item (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	CreateItemFunc rfunc;
	GnomeCanvasItem **retval;

	retval = (GnomeCanvasItem **) GTK_RETLOC_POINTER (args[1]);
	rfunc = (CreateItemFunc) func;
	*retval = (* rfunc) (object, GTK_VALUE_OBJECT (args[0]), data);
}

typedef void (* ConfigureItemFunc) (GtkObject *object, GtkObject *item,
				    GtkObject *model, guint n,
				    gboolean is_selected, gboolean is_focused,
				    gpointer data);

static void
marshal_configure_item (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	ConfigureItemFunc rfunc;

	rfunc = (ConfigureItemFunc) func;
	(* func) (object, GTK_VALUE_OBJECT (args[0]),
		  GTK_VALUE_OBJECT (args[1]), GTK_VALUE_UINT (args[2]),
		  GTK_VALUE_BOOL (args[3]), GTK_VALUE_BOOL (args[4]),
		  data);
}

typedef void (* GetItemSizeFunc) (GtkObject *object, GtkObject *item, GtkObject *model, guint n,
				  gpointer width, gpointer height);

static void
marshal_get_item_size (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	GetItemSizeFunc rfunc;

	rfunc = (GetItemSizeFunc) func;
	(* rfunc) (object, GTK_VALUE_OBJECT (args[0]),
		   GTK_VALUE_OBJECT (args[1]), GTK_VALUE_UINT (args[2]),
		   GTK_VALUE_POINTER (args[3]), GTK_VALUE_POINTER (args[4]));
}



/* Exported functions */

/**
 * gnome_list_item_factory_create_item:
 * @factory: A list item factory.
 * @parent: Canvas group to act as the item's parent.
 *
 * Makes a list item factory create an empty item.
 *
 * Return value: An canvas item representing an empty list item.
 **/
GnomeCanvasItem *
gnome_list_item_factory_create_item (GnomeListItemFactory *factory, GnomeCanvasGroup *parent)
{
	GnomeCanvasItem *retval;

	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_LIST_ITEM_FACTORY (factory), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	retval = NULL;
	gtk_signal_emit (GTK_OBJECT (factory), li_factory_signals[CREATE_ITEM],
			 parent, &retval);
	return retval;
}

/**
 * gnome_list_item_factory_configure_item:
 * @factory: A list item factory.
 * @item: A list item created by this factory.
 * @model: A list model.
 * @n: Index in the list model.
 * @is_selected: Whether the item is selected or not.
 * @is_focused: Whether the item has the focus or not.
 *
 * Requests that a list item generated by a list item factory be configured for
 * a particular element in the list model, as well as with a certain selection
 * and focus state.
 **/
void
gnome_list_item_factory_configure_item (GnomeListItemFactory *factory, GnomeCanvasItem *item,
					GnomeListModel *model, guint n,
					gboolean is_selected, gboolean is_focused)
{
	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_LIST_ITEM_FACTORY (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_MODEL (model));
	g_return_if_fail (n < gnome_list_model_get_length (model));

	gtk_signal_emit (GTK_OBJECT (factory), li_factory_signals[CONFIGURE_ITEM],
			 item, model, n, is_selected, is_focused);
}

/**
 * gnome_list_item_factory_get_item_size:
 * @factory: A list item factory.
 * @item: If non-NULL, a list item created by this factory.
 * @model: A list model.
 * @n: Index in the list model of the item to measure.
 * @width: Return value for the item's width.
 * @height: Return value for item's height.
 *
 * Queries the size in pixels of a list item that was or would be created by the
 * specified factory.  If the specified @item is not NULL, then the factory can
 * use it to compute the size more efficiently.  Otherwise, the factory will
 * have to compute the size from scratch.
 **/
void
gnome_list_item_factory_get_item_size (GnomeListItemFactory *factory, GnomeCanvasItem *item,
				       GnomeListModel *model, guint n,
				       int *width, int *height)
{
	int w, h;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_LIST_ITEM_FACTORY (factory));

	if (item)
		g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_LIST_MODEL (model));
	g_return_if_fail (n < gnome_list_model_get_length (model));

	w = h = 0;
	gtk_signal_emit (GTK_OBJECT (factory), li_factory_signals[GET_ITEM_SIZE],
			 item, model, n, &w, &h);

	if (width)
		*width = w;

	if (height)
		*height = h;
}
