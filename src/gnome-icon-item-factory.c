/* GNOME libraries - default item factory for icon lists
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
#include "gnome-canvas-pixbuf.h"
#include "gnome-icon-item-factory.h"
#include "gnome-icon-list-model.h"



/* Private part of the GnomeIconItemFactory structure */
typedef struct {
	/* Size of items */
	int item_width;
	int item_height;

	/* Size of icon images */
	int image_width;
	int image_height;
} IIFactoryPrivate;



/* Icon item structure */
typedef struct {
	/* Base group */
	GnomeCanvasItem *item;

	/* Icon image and its selection and focus rectangles */
	GnomeCanvasItem *image;
	GnomeCanvasItem *sel_image;
	GnomeCanvasItem *focus_image;

	/* Caption and its selection and focus rectangles */
	GnomeCanvasItem *caption;
	GnomeCanvasItem *sel_caption;
	GnomeCanvasItem *focus_caption;
} IconItem;



static void gnome_icon_item_factory_class_init (GnomeIconItemFactoryClass *class);
static void gnome_icon_item_factory_init (GnomeIconItemFactory *factory);
static void gnome_icon_item_factory_destroy (GtkObject *object);

static GnomeCanvasItem *ii_factory_create_item (GnomeListItemFactory *factory,
						GnomeCanvasGroup *parent);
static void ii_factory_configure_item (GnomeListItemFactory *factory, GnomeCanvasItem *item,
				       GnomeListModel *model, guint n,
				       gboolean is_selected, gboolean is_focused);
static void ii_factory_get_item_size (GnomeListItemFactory *factory, GnomeCanvasItem *item,
				      GnomeListModel *model, guint n,
				      gint *width, gint *height);

static GnomeListItemFactoryClass *parent_class;



/**
 * gnome_icon_item_factory_get_type:
 * @void:
 *
 * Registers the &GnomeIconItemFactory class if necessary, and returns the type
 * ID associated to it.
 *
 * Return value: The type ID of the &GnomeIconItemFactory class.
 **/
GtkType
gnome_icon_item_factory_get_type (void)
{
	static GtkType ii_factory_type = 0;

	if (!ii_factory_type) {
		static const GtkTypeInfo ii_factory_info = {
			"GnomeIconItemFactory",
			sizeof (GnomeIconItemFactory),
			sizeof (GnomeIconItemFactoryClass),
			(GtkClassInitFunc) gnome_icon_item_factory_class_init,
			(GtkObjectInitFunc) gnome_icon_item_factory_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ii_factory_type = gtk_type_unique (gnome_list_item_factory_get_type (),
						   &ii_factory_info);
	}

	return ii_factory_type;
}

/* Class initialization function for the icon list item factory */
static void
gnome_icon_item_factory_class_init (GnomeIconItemFactoryClass *class)
{
	GtkObjectClass *object_class;
	GnomeListItemFactoryClass *li_factory_class;

	object_class = (GtkObjectClass *) class;
	li_factory_class = (GnomeListItemFactoryClass *) class;

	parent_class = gtk_type_class (gnome_list_item_factory_get_type ());

	object_class->destroy = gnome_icon_item_factory_destroy;

	li_factory_class->create_item = ii_factory_create_item;
	li_factory_class->configure_item = ii_factory_configure_item;
	li_factory_class->get_item_size = ii_factory_get_item_size;
}

/* Object initialization function for the icon list item factory */
static void
gnome_icon_item_factory_init (GnomeIconItemFactory *factory)
{
	IIFactoryPrivate *priv;

	priv = g_new0 (IIFactoryPrivate, 1);
	factory->priv = priv;
}

/* Destroy handler for the icon list item factory */
static void
gnome_icon_item_factory_destroy (GtkObject *object)
{
	GnomeIconItemFactory *factory;
	IIFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (object));

	factory = GNOME_ICON_ITEM_FACTORY (object);
	priv = factory->priv;

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Default signal handlers */

/* Called when an icon's main group is destroyed */
static void
icon_destroyed (GtkObject *object, gpointer data)
{
	IconItem *icon;

	icon = data;
	g_free (icon);
}

/* Create_item handler for the icon list item factory */
static GnomeCanvasItem *
ii_factory_create_item (GnomeListItemFactory *factory, GnomeCanvasGroup *parent)
{
	IconItem *icon;

	g_return_val_if_fail (factory != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory), NULL);
	g_return_val_if_fail (parent != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (parent), NULL);

	icon = g_new (IconItem, 1);

	icon->item = gnome_canvas_item_new (parent,
					    GNOME_TYPE_CANVAS_GROUP,
					    NULL);
	gtk_object_set_data (GTK_OBJECT (icon->item), "IconItem", icon);
	gtk_signal_connect (GTK_OBJECT (icon->item), "destroy",
			    icon_destroyed,
			    icon);

	/* Image */

	icon->image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					     GNOME_TYPE_CANVAS_PIXBUF,
					     NULL);
	gnome_canvas_item_hide (icon->image);

	icon->sel_image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						 GNOME_TYPE_CANVAS_RECT,
						 NULL);
	gnome_canvas_item_hide (icon->sel_image);

	icon->focus_image = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						   GNOME_TYPE_CANVAS_RECT,
						   NULL);
	gnome_canvas_item_hide (icon->focus_image);

	/* Caption */

	icon->sel_caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						   GNOME_TYPE_CANVAS_RECT,
						   NULL);
	gnome_canvas_item_hide (icon->sel_caption);

	icon->caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
					       GNOME_TYPE_CANVAS_TEXT,
					       NULL);
	gnome_canvas_item_hide (icon->caption);

	icon->focus_caption = gnome_canvas_item_new (GNOME_CANVAS_GROUP (icon->item),
						     GNOME_TYPE_CANVAS_RECT,
						     NULL);
	gnome_canvas_item_hide (icon->focus_caption);

	return icon->item;
}

/* Configure_item handler for the icon list item factory */
static void
ii_factory_configure_item (GnomeListItemFactory *factory, GnomeCanvasItem *item,
			   GnomeListModel *model, guint n,
			   gboolean is_selected, gboolean is_focused)
{
	IconItem *icon;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory));
	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));
	g_return_if_fail (n < gnome_list_model_get_length (model));

	icon = gtk_object_get_data (GTK_OBJECT (item), "IconItem");
	g_assert (icon != NULL);

	
}

/* Get_item_size handler for the icon list item factory */
static void
ii_factory_get_item_size (GnomeListItemFactory *factory, GnomeCanvasItem *item,
			  GnomeListModel *model, guint n,
			  gint *width, gint *height)
{
	/* FIXME */
}



/* Exported functions */

/**
 * gnome_icon_item_factory_set_item_metrics:
 * @factory: An icon list item factory.
 * @item_width: Total width of items, in pixels.
 * @item_height: Total height of items, in pixels.
 * @image_width: Maximum width of images for icons, in pixels.
 * @image_height: Maximum height of images for icons, in pixels.
 *
 * Sets the metrics of the items that will be created in the future by an icon
 * list item factory.  This includes the total size of items and the maximum
 * size of icon images.
 **/
void
gnome_icon_item_factory_set_item_metrics (GnomeIconItemFactory *factory,
					  int item_width, int item_height,
					  int image_width, int image_height)
{
	IIFactoryPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory));
	g_return_if_fail (item_width > 0);
	g_return_if_fail (item_height > 0);
	g_return_if_fail (image_width <= item_width);
	g_return_if_fail (image_height <= item_height);

	priv = factory->priv;
	priv->item_width = item_width;
	priv->item_height = item_height;
	priv->image_width = image_width;
	priv->image_height = image_height;
}

/**
 * gnome_icon_item_factory_get_item_metrics:
 * @factory: An icon list item factory.
 * @item_width: Return value for the total width of items, in pixels.
 * @item_height: Return value for the total height of items, in pixels.
 * @image_width: Return value for the maximum width of icon images, in pixels.
 * @image_height: Return value for the maximum height of icon images, in pixels.
 *
 * Queries the metrics of items that an icon list item factory is configured to
 * create.
 **/
void
gnome_icon_item_factory_get_item_metrics (GnomeIconItemFactory *factory,
					  int *item_width, int *item_height,
					  int *image_width, int *image_height)
{
	IIFactoryPrivate *priv;

	g_return_if_fail (factory != NULL);
	g_return_if_fail (GNOME_IS_ICON_ITEM_FACTORY (factory));

	priv = factory->priv;

	if (item_width)
		*item_width = priv->item_width;

	if (item_height)
		*item_height = priv->item_height;

	if (image_widht)
		*image_width = priv->image_width;

	if (image_height)
		*image_height = priv->image_height;
}
