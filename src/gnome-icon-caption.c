/* GNOME libraries - canvas item for icon captions
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gnu.org>
 *          Miguel de Icaza <miguel@gnu.org>
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
#include "gnome-icon-caption.h"



/* Private part of the GnomeIconCaption structure */
typedef struct {
	/* Maximum size */
	int max_width;

	/* Maximum number of displayed lines when not focused or editing */
	int max_lines;

	/* HACK, FIXME:  This uses an offscreen entry widget to process
	 * keystrokes and maintain the text while editing.
	 */
	GtkEntry *entry;
	GtkEntry *entry_window;

	/* Whether we are selected */
	guint is_selected : 1;

	/* Whether we are focused */
	guint is_focused : 1;

	/* Whether we are being edited */
	guint is_editing : 1;

	/* Whether selected/focused/edited changed */
	guint need_state_Update : 1;

	/* Whether the text changed */
	guint need_text_update : 1;
} IconCaptionPrivate;



/* Object argument IDs */
enum {
	ARG_0,
	ARG_IS_SELECTED,
	ARG_IS_FOCUSED,
	ARG_IS_EDITING,
	ARG_EDITED_TEXT
};

/* Signal IDs */

enum {
	REQUEST_TEXT,
	LAST_SIGNAL
};

static void gnome_icon_caption_class_init (GnomeIconCaptionClass *class);
static void gnome_icon_caption_init (GnomeIconCaption *caption);
static void gnome_icon_caption_destroy (GtkObject *object);
static void gnome_icon_caption_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gnome_icon_caption_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void gnome_icon_caption_update (GnomeCanvasItem *item, double *affine,
				       ArtSVP *clip_path, int flags);
static void gnome_icon_caption_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				     int x, int y, int width, int height);
statid double gnome_icon_caption_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
					GnomeCanvasItem **actual_item);
static void gnome_icon_caption_bounds (GnomeCanvasItem *item,
				       double *x1, double *y1, double *x2, double *y2);

static void marshal_request_text (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args);

static GnomeCanvasItemClass *parent_class;

static guint caption_signals[LAST_SIGNAL];



/**
 * gnome_icon_caption_get_type:
 * @void:
 *
 * Registers the #GnomeIconCaption class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeIconCaption class.
 **/
GtkType
gnome_icon_caption_get_type (void)
{
	static GtkType icon_caption_type = 0;

	if (!icon_caption_type) {
		static const GtkTypeInfo icon_caption_info = {
			"GnomeIconCaption",
			sizeof (GnomeIconCaption),
			sizeof (GnomeIconCaptionClass),
			(GtkClassInitfunc) gnome_icon_caption_class_init,
			(GtkObjectInitFunc) gnome_icon_caption_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		icon_caption_type = gtk_type_unique (gnome_canvas_item_get_type (),
						     &icon_caption_info);
	}

	return icon_caption_type;
}

/* Class initialization function for the icon caption item */
static void
gnome_icon_caption_class_init (GnomeIconCaptionClass *class)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	gtk_object_add_arg_type ("GnomeIconCaption::is_selected",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_IS_SELECTED);
	gtk_object_add_arg_type ("GnomeIconCaption::is_focused",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_IS_FOCUSED);
	gtk_object_add_arg_type ("GnomeIconCaption::is_editing",
				 GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_IS_EDITING);
	gtk_object_add_arg_type ("GnomeIconCaption::edited_text",
				 GTK_TYPE_STRING, GTK_ARG_READABLE, ARG_EDITED_TEXT);

	caption_signals[REQUEST_TEXT] =
		gtk_signal_new ("request_text",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GnomeIconCaptionClass, request_text),
				marshal_request_text,
				GTK_TYPE_STRING, 0);

	gtk_object_class_add_signals (object_class, caption_signals, LAST_SIGNAL);

	object_class->destroy = gnome_icon_caption_destroy;
	object_class->set_arg = gnome_icon_caption_set_arg;
	object_class->get_arg = gnome_icon_caption_get_arg;

	item_class->update = gnome_icon_caption_update;
	item_class->draw = gnome_icon_caption_draw;
	item_class->point = gnome_icon_caption_point;
	item_class->bounds = gnome_icon_caption_bounds;
}

/* Object initialization function for the icon caption item */
static void
gnome_icon_caption_init (GnomeIconCaption *caption)
{
	IconCaptionPrivate *priv;

	priv = g_new0 (IconCaptionPrivate, 1);
	caption->priv = priv;

	/* Semi-sane defaults */
	priv->max_width = 80;
	priv->max_lines = 2;
}

/* Destroy handler for the icon caption item */
static void
gnome_icon_caption_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	GnomeIconCaption *caption;
	IconCaptionPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_ICON_CAPTION (object));

	item = GNOME_CANVAS_ITEM (object);
	caption = GNOME_ICON_CAPTION (object);
	priv = caption->priv;

	/* FIXME: redraw old area */

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Set_arg handler for the icon caption item */
static void
gnome_icon_caption_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	GnomeIconCaption *caption;
	IconCaptionPrivate *priv;

	item = GNOME_CANVAS_ITEM (object);
	caption = GNOME_ICON_CAPTION (object);
	priv = caption->priv;

	switch (arg_id) {
	case ARG_IS_SELECTED:
		priv->is_selected = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_state_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_IS_FOCUSED:
		priv->is_focused = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_state_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case ARG_IS_EDITING:
		/* FIXME: destroy entry in ::update() */
		priv->is_editing = GTK_VALUE_BOOL (*arg) ? TRUE : FALSE;
		priv->need_state_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}

/* Get_arg handler for the icon caption item */
static void
gnome_icon_caption_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GnomeIconCaption *caption;
	IconCaptionPrivate *priv;

	caption = GNOME_ICON_CAPTION (object);
	priv = caption->priv;

	switch (arg_id) {
	case ARG_IS_SELECTED:
		GTK_VALUE_BOOL (*arg) = priv->is_selected;
		break;

	case ARG_IS_FOCUSED:
		GTK_VALUE_BOOL (*arg) = priv->is_focused;
		break;

	case ARG_IS_EDITING:
		GTK_VALUE_BOOL (*arg) = priv->is_editing;
		break;

	case ARG_EDITED_TEXT:
		if (!priv->is_editing)
			GTK_VALUE_STRING (*arg) = NULL;
		else {
			g_assert (priv->entry != NULL);
			GTK_VALUE_STRING (*arg) = gtk_editable_get_chars (
				GTK_EDITABLE (priv->entry), 0, -1);
		}
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}



/* Marshalers */

typedef const char *(* RequestTextfunc) (GtkObject *object, gpointer data);

static void
marshal_request_text (GtkObject *object, GtkSignalFunc func, gpointer data, GtkArg *args)
{
	RequestTextFunc rfunc;
	const char **retval;

	retval = GTK_RETLOC_STRING (args[0]);
	rfunc = (RequestTextFunc) func;
	*retval = (* rfunc) (object, data);
}



/* Exported functions */

/**
 * gnome_icon_caption_text_changed:
 * @caption: An icon caption item.
 * 
 * Notifies an icon caption item that the text it displays has changed.  This
 * should be used by applications when they change the text that an icon caption
 * item should display.  The icon caption item will emit the "request_text"
 * signal when it needs to redraw itself to ask the application for the text it
 * needs.
 **/
void
gnome_icon_caption_text_changed (GnomeIconCaption *caption)
{
	IconCaptionPrivate *priv;

	g_return_if_fail (caption != NULL);
	g_return_if_fail (GNOME_IS_ICON_CAPTION (caption));

	priv = caption->priv;

	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (caption));
}
