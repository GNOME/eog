/* GNOME libraries - abstract icon list model
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
#include <gtk/gtksignal.h>
#include "gnome-icon-list-model.h"



/* Signal IDs */
enum {
	GET_ICON,
	LAST_SIGNAL
};

static void gnome_icon_list_model_class_init (GnomeIconListModelClass *class);

static void marshal_get_icon (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);

static guint ilm_signals[LAST_SIGNAL];



/**
 * gnome_icon_list_model_get_type:
 * @void:
 *
 * Registers the #GnomeIconListModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeIconListModel class.
 **/
GtkType
gnome_icon_list_model_get_type (void)
{
	static GtkType ilm_type = 0;

	if (!ilm_type) {
		static const GtkTypeInfo ilm_info = {
			"GnomeIconListModel",
			sizeof (GnomeIconListModel),
			sizeof (GnomeIconListModelClass),
			(GtkClassInitFunc) gnome_icon_list_model_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		ilm_type = gtk_type_unique (gnome_list_model_get_type (), &ilm_info);
	}

	return ilm_type;
}

/* Class initialization function for the abstract icon list model */
static void
gnome_icon_list_model_class_init (GnomeIconListModelClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	ilm_signals[GET_ICON] =
		gtk_signal_new ("get_icon",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeIconListModelClass, get_icon),
				marshal_get_icon,
				GTK_TYPE_NONE, 3,
				GTK_TYPE_UINT,
				GTK_TYPE_POINTER,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, ilm_signals, LAST_SIGNAL);
}



/* Marshalers */

typedef void (* GetIconFunc) (GtkObject *object, guint n, gpointer pixbuf, gpointer caption,
			      gpointer data);

static void
marshal_get_icon (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	GetIconFunc rfunc;

	rfunc = (GetIconFunc) func;
	(* rfunc) (object, GTK_VALUE_UINT (args[0]), GTK_VALUE_POINTER (args[1]),
		   GTK_VALUE_POINTER (args[2]), data);
}



/* Exported functions */

/**
 * gnome_icon_list_model_get_icon:
 * @model: An icon list model.
 * @n: Index of item to query.
 * @pixbuf: Return value for the icon's image.
 * @caption: Return value for the icon's caption.
 *
 * Queries the image and caption data for an icon in an icon list model.
 **/
void
gnome_icon_list_model_get_icon (GnomeIconListModel *model, guint n,
				GdkPixbuf **pixbuf, const char **caption)
{
	GdkPixbuf *p;
	const char *c;

	g_return_if_fail (model != NULL);
	g_return_if_fail (GNOME_IS_ICON_LIST_MODEL (model));

	p = NULL;
	c = NULL;

	gtk_signal_emit (GTK_OBJECT (model), ilm_signals[GET_ICON],
			 n, &p, &c);

	if (pixbuf)
		*pixbuf = p;

	if (caption)
		*caption = c;
}
