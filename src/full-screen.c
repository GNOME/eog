/* Eye of Gnome image viewer - full-screen view mode
 *
 * Copyright (C) 2000 The Free Software Foundation
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include "full-screen.h"
#include "image-view.h"
#include "ui-image.h"



/* Private part of the FullScreen structure */
typedef struct {
	/* Scrolling user interface for the image */
	GtkWidget *ui;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;
} FullScreenPrivate;



static void full_screen_class_init (FullScreenClass *class);
static void full_screen_init (FullScreen *fs);
static void full_screen_finalize (GtkObject *object);

static void full_screen_show (GtkWidget *widget);
static void full_screen_hide (GtkWidget *widget);

static gint full_screen_key_press (GtkWidget *widget, GdkEventKey *event);

static GtkWindowClass *parent_class;



/**
 * full_screen_get_type:
 * @void: 
 * 
 * Registers the #FullScreen class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #FullScreen class.
 **/
GtkType
full_screen_get_type (void)
{
	static GtkType full_screen_type = 0;

	if (!full_screen_type) {
		static GtkTypeInfo full_screen_info = {
			"FullScreen",
			sizeof (FullScreen),
			sizeof (FullScreenClass),
			(GtkClassInitFunc) full_screen_class_init,
			(GtkObjectInitFunc) full_screen_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		full_screen_type = gtk_type_unique (GTK_TYPE_WINDOW, &full_screen_info);
	}

	return full_screen_type;
}

/* Class initialization function for the full screen mode */
static void
full_screen_class_init (FullScreenClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WINDOW);

	object_class->finalize = full_screen_finalize;

	widget_class->show = full_screen_show;
	widget_class->hide = full_screen_hide;
	widget_class->key_press_event = full_screen_key_press;
}

/* Object initialization function for the full screen view */
static void
full_screen_init (FullScreen *fs)
{
	FullScreenPrivate *priv;

	priv = g_new0 (FullScreenPrivate, 1);
	fs->priv = priv;

	GTK_WINDOW (fs)->type = GTK_WINDOW_POPUP;
	gtk_widget_set_usize (GTK_WIDGET (fs), gdk_screen_width (), gdk_screen_height ());
	gtk_widget_set_uposition (GTK_WIDGET (fs), 0, 0);

	priv->ui = ui_image_new ();
	gtk_container_add (GTK_CONTAINER (fs), priv->ui);
	gtk_widget_show (priv->ui);
}

/* Finalize handler for the full screen view */
static void
full_screen_finalize (GtkObject *object)
{
	FullScreen *fs;
	FullScreenPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_FULL_SCREEN (object));

	fs = FULL_SCREEN (object);
	priv = fs->priv;

	g_free (priv);
	fs->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Widget methods */

/* Show handler for the full screen view */
static void
full_screen_show (GtkWidget *widget)
{
	FullScreen *fs;
	FullScreenPrivate *priv;

	fs = FULL_SCREEN (widget);
	priv = fs->priv;

	if (GTK_WIDGET_CLASS (parent_class)->show)
		(* GTK_WIDGET_CLASS (parent_class)->show) (widget);

	priv->have_grab = gdk_keyboard_grab (widget->window, TRUE, GDK_CURRENT_TIME) == 0;
	gtk_grab_add (widget);

	gtk_widget_grab_focus (ui_image_get_image_view (UI_IMAGE (priv->ui)));
}

/* Hide handler for the full screen view */
static void
full_screen_hide (GtkWidget *widget)
{
	FullScreen *fs;
	FullScreenPrivate *priv;

	fs = FULL_SCREEN (widget);
	priv = fs->priv;

	if (priv->have_grab) {
		priv->have_grab = FALSE;
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	}

	if (GTK_WIDGET_CLASS (parent_class)->show)
		(* GTK_WIDGET_CLASS (parent_class)->show) (widget);

	gtk_widget_destroy (widget);
}

/* Key press handler for the full screen view */
static gint
full_screen_key_press (GtkWidget *widget, GdkEventKey *event)
{
	gint result;
	gboolean do_hide;

	result = FALSE;

	if (GTK_WIDGET_CLASS (parent_class)->key_press_event)
		result = (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);

	if (result)
		return result;

	do_hide = FALSE;

	switch (event->keyval) {
	case GDK_Escape:
		do_hide = TRUE;
		break;

	case GDK_W:
	case GDK_w:
		if (event->state & GDK_CONTROL_MASK)
			do_hide = TRUE;
		break;

	case GDK_Q:
	case GDK_q:
		do_hide = TRUE;
		break;

	default:
		return FALSE;
	}

	if (do_hide)
		gtk_widget_hide (widget);

	return TRUE;
}



/**
 * full_screen_new:
 * @void: 
 * 
 * Creates a new empty full screen image viewer.
 * 
 * Return value: A newly-created full screen image viewer.
 **/
GtkWidget *
full_screen_new (void)
{
	return GTK_WIDGET (gtk_type_new (TYPE_FULL_SCREEN));
}

/**
 * full_screen_set_image:
 * @fs: A full screen view.
 * @image: An image structure, or NULL if none.
 * 
 * Sets the image to be displayed in a full screen image view.
 **/
void
full_screen_set_image (FullScreen *fs, Image *image)
{
	FullScreenPrivate *priv;
	GtkWidget *view;

	g_return_if_fail (fs != NULL);
	g_return_if_fail (IS_FULL_SCREEN (fs));

	priv = fs->priv;

	view = ui_image_get_image_view (UI_IMAGE (priv->ui));
	image_view_set_image (IMAGE_VIEW (view), image);
}

/**
 * full_screen_get_ui_image:
 * @fs: A full screen view.
 * 
 * Queries the image view scroller inside a full screen image view.
 * 
 * Return value: An image view scroller.
 **/
GtkWidget *
full_screen_get_ui_image (FullScreen *fs)
{
	FullScreenPrivate *priv;

	g_return_val_if_fail (fs != NULL, NULL);
	g_return_val_if_fail (IS_FULL_SCREEN (fs), NULL);

	priv = fs->priv;
	return priv->ui;
}
