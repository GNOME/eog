/* Eye Of Gnome - abstract item factory
 *
 * Copyright (C) 2000-2001 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
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
#include <gtk/gtksignal.h>
#include "eog-item-factory.h"



/* Signal IDs */
enum {
	CREATE_ITEM,
	UPDATE_ITEM,
	GET_ITEM_SIZE,
	CONFIGURATION_CHANGED,
	LAST_SIGNAL
};

static void eog_item_factory_class_init (EogItemFactoryClass *class);

static void marshal_create_item (GtkObject *object, GtkSignalFunc func,
				 gpointer data, GtkArg *args);
static void marshal_update_item (GtkObject *object, GtkSignalFunc func,
				 gpointer data, GtkArg *args);
static void marshal_get_item_size (GtkObject *object, GtkSignalFunc func,
				   gpointer data, GtkArg *args);

static guint ei_factory_signals[LAST_SIGNAL];



/**
 * eog_item_factory_get_type:
 * @void:
 *
 * Registers the #EogItemFactory class if necessary, and returns the type
 * ID associated to it.
 *
 * Return value: The type ID of the #GnomeListItemFactory class.
 **/
GtkType
eog_item_factory_get_type (void)
{
	static GtkType ei_factory_type = 0;

	if (!ei_factory_type) {
		static const GtkTypeInfo ei_factory_info = {
			"EogItemFactory",
			sizeof (EogItemFactory),
			sizeof (EogItemFactoryClass),
			(GtkClassInitFunc) eog_item_factory_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ei_factory_type = gtk_type_unique (gtk_object_get_type (), &ei_factory_info);
	}

	return ei_factory_type;
}

/* Class initialization function for the abstract list item factory */
static void
eog_item_factory_class_init (EogItemFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	ei_factory_signals[CREATE_ITEM] =
		gtk_signal_new ("create_item",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogItemFactoryClass, create_item),
				marshal_create_item,
				GNOME_TYPE_CANVAS_ITEM, 2,
				GNOME_TYPE_CANVAS_GROUP,
				GTK_TYPE_UINT);
	ei_factory_signals[UPDATE_ITEM] =
		gtk_signal_new ("update_item",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogItemFactoryClass, update_item),
				marshal_update_item,
				GTK_TYPE_NONE, 2,
				EOG_COLLECTION_MODEL_TYPE,
				GNOME_TYPE_CANVAS_ITEM);
	ei_factory_signals[GET_ITEM_SIZE] =
		gtk_signal_new ("get_item_size",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogItemFactoryClass, get_item_size),
				marshal_get_item_size,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER,
				GTK_TYPE_POINTER);
	ei_factory_signals[CONFIGURATION_CHANGED] =
		gtk_signal_new ("configuration_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EogItemFactoryClass, configuration_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, ei_factory_signals, LAST_SIGNAL);
}



/* Marshalers */

typedef GnomeCanvasItem *(* CreateItemFunc) (GtkObject *object, GtkObject *parent, 
					     guint id, gpointer data);

static void
marshal_create_item (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	CreateItemFunc rfunc;
	GnomeCanvasItem **retval;

	retval = (GnomeCanvasItem **) GTK_RETLOC_POINTER (args[2]);
	rfunc = (CreateItemFunc) func;
	*retval = (* rfunc) (object, GTK_VALUE_OBJECT (args[0]), GTK_VALUE_UINT (args[1]), data);
}

typedef void (* UpdateItemFunc) (GtkObject *object, 
				 GtkObject *model, 
				 GtkObject *item,
				 gpointer data);

static void
marshal_update_item (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	UpdateItemFunc rfunc;

	rfunc = (UpdateItemFunc) func;
	(* func) (object, GTK_VALUE_OBJECT (args[0]),
		  GTK_VALUE_OBJECT (args[1]), data);
}

typedef void (* GetItemSizeFunc) (GtkObject *object, 
				  gpointer width, gpointer height,
				  gpointer data);

static void
marshal_get_item_size (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	GetItemSizeFunc rfunc;

	rfunc = (GetItemSizeFunc) func;
	(* rfunc) (object,
		   GTK_VALUE_POINTER (args[0]), GTK_VALUE_POINTER (args[1]), data);
}



/* Exported functions */

/**
 * eog_item_factory_create_item:
 * @factory: A list item factory.
 * @parent: Canvas group to act as the item's parent.
 *
 * Makes a list item factory create an empty item.
 *
 * Return value: An canvas item representing an empty list item.
 **/
GnomeCanvasItem *
eog_item_factory_create_item (EogItemFactory *factory, GnomeCanvasGroup *parent, 
			      guint id)
{
	GnomeCanvasItem *retval;

	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (EOG_IS_ITEM_FACTORY (factory), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	retval = NULL;
	gtk_signal_emit (GTK_OBJECT (factory), ei_factory_signals[CREATE_ITEM],
			 parent, id, &retval);
	return retval;
}

/**
 * eog_item_factory_configure_item:
 * @factory: A list item factory.
 * @model: A list model.
 * @item: A list item created by this factory.

 * Requests that a list item generated by a list item factory be configured for
 * a particular element in the list model, as well as with a certain selection
 * and focus state.
 **/
void
eog_item_factory_update_item (EogItemFactory *factory, 
			      EogCollectionModel *model, 
			      GnomeCanvasItem *item)
{
	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (EOG_IS_COLLECTION_MODEL (model));

	gtk_signal_emit (GTK_OBJECT (factory), ei_factory_signals[UPDATE_ITEM],
			 model, item);
}

/**
 * eog_item_factory_get_item_size:
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
eog_item_factory_get_item_size (EogItemFactory *factory,
				int *width, int *height)
{
	int w, h;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY (factory));

	w = h = 0;
	gtk_signal_emit (GTK_OBJECT (factory), ei_factory_signals[GET_ITEM_SIZE],
			 &w, &h);

	if (width)
		*width = w;

	if (height)
		*height = h;
}

void
eog_item_factory_configuration_changed (EogItemFactory *factory)
{
	g_return_if_fail (factory != NULL);
	g_return_if_fail (EOG_IS_ITEM_FACTORY (factory));

	gtk_signal_emit (GTK_OBJECT (factory), ei_factory_signals[CONFIGURATION_CHANGED]);
}
