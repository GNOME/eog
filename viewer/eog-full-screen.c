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

#include "eog-full-screen.h"

#include <image-view.h>
#include <ui-image.h>

struct _EogFullScreenPrivate
{
	EogImageView *image_view;

	/* Whether we have a keyboard grab */
	guint have_grab : 1;
};

#define ZOOM_FACTOR 1.2
static GtkWindowClass *parent_class;

/* Show handler for the full screen view */
static void
eog_full_screen_show (GtkWidget *widget)
{
	EogFullScreen *fs;
	GtkWidget *ui_image;
	GtkWidget *image_view;

	fs = EOG_FULL_SCREEN (widget);

	if (GTK_WIDGET_CLASS (parent_class)->show)
		(* GTK_WIDGET_CLASS (parent_class)->show) (widget);

	fs->priv->have_grab = !gdk_keyboard_grab (widget->window,
			                          TRUE, GDK_CURRENT_TIME);
	gtk_grab_add (widget);

	ui_image = eog_image_view_get_widget (fs->priv->image_view);
	image_view = ui_image_get_image_view (UI_IMAGE (ui_image));
	gtk_widget_unref (ui_image);

	gtk_widget_grab_focus (image_view);
}

/* Hide handler for the full screen view */
static void
eog_full_screen_hide (GtkWidget *widget)
{
	EogFullScreen *fs;
	
	fs = EOG_FULL_SCREEN (widget);

	if (fs->priv->have_grab) {
		fs->priv->have_grab = FALSE;
		gdk_keyboard_ungrab (GDK_CURRENT_TIME);
	}

	if (GTK_WIDGET_CLASS (parent_class)->show)
		(* GTK_WIDGET_CLASS (parent_class)->show) (widget);

	gtk_widget_destroy (widget);
}

/* Key press handler for the full screen view */
static gint
eog_full_screen_key_press (GtkWidget *widget, GdkEventKey *event)
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
	case GDK_Q:
	case GDK_q:
	case GDK_Escape:
		do_hide = TRUE;
		break;

	case GDK_W:
	case GDK_w:
		if (event->state & GDK_CONTROL_MASK)
			do_hide = TRUE;
		break;

	default:
		return FALSE;
	}

	if (do_hide)
		gtk_widget_hide (widget);

	return TRUE;
}

static gint
eog_full_screen_button_press_event (GtkWidget *widget,
		                    GdkEventButton *event_button)
{
	EogFullScreen *fs;
	gfloat zoom_factor;

	fs = EOG_FULL_SCREEN (widget);

	if (event_button->button == 2) {

		/* Zoom in or out */
		zoom_factor = eog_image_view_get_zoom_factor
							(fs->priv->image_view);
		if (event_button->state & GDK_SHIFT_MASK)
			zoom_factor /= ZOOM_FACTOR;
		else
			zoom_factor *= ZOOM_FACTOR;
		eog_image_view_set_zoom_factor (fs->priv->image_view,
						zoom_factor);
		return (TRUE);
	}

	return (FALSE);
}

static void
eog_full_screen_destroy (GtkObject *object)
{
	EogFullScreen *fs;
	
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);

	bonobo_object_unref (BONOBO_OBJECT (fs->priv->image_view));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}	

/* Finalize handler for the full screen view */
static void
eog_full_screen_finalize (GtkObject *object)
{
	EogFullScreen *fs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EOG_IS_FULL_SCREEN (object));

	fs = EOG_FULL_SCREEN (object);

	g_free (fs->priv);

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Class initialization function for the full screen mode */
static void
eog_full_screen_class_init (EogFullScreenClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WINDOW);

	object_class->destroy = eog_full_screen_destroy;
	object_class->finalize = eog_full_screen_finalize;

	widget_class->show = eog_full_screen_show;
	widget_class->hide = eog_full_screen_hide;
	widget_class->key_press_event = eog_full_screen_key_press;
	widget_class->button_press_event = eog_full_screen_button_press_event;
}

/* Object initialization function for the full screen view */
static void
eog_full_screen_init (EogFullScreen *fs)
{
	fs->priv = g_new0 (EogFullScreenPrivate, 1);

	GTK_WINDOW (fs)->type = GTK_WINDOW_POPUP;
	gtk_widget_set_usize (GTK_WIDGET (fs),
			      gdk_screen_width (), gdk_screen_height ());
	gtk_widget_set_uposition (GTK_WIDGET (fs), 0, 0);
}

/**
 * eog_full_screen_get_type:
 * @void:
 *
 * Registers the #FullScreen class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #FullScreen class.
 **/
GtkType
eog_full_screen_get_type (void)
{
	static GtkType eog_full_screen_type = 0;

	if (!eog_full_screen_type) {
		static GtkTypeInfo eog_full_screen_info = {
			"EogFullScreen",
			sizeof (EogFullScreen),
			sizeof (EogFullScreenClass),
			(GtkClassInitFunc) eog_full_screen_class_init,
			(GtkObjectInitFunc) eog_full_screen_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		eog_full_screen_type = gtk_type_unique (GTK_TYPE_WINDOW,
				                        &eog_full_screen_info);
	}

	return eog_full_screen_type;
}

GtkWidget *
eog_full_screen_new (EogImage *image)
{
	EogFullScreen *fs;
	GtkWidget     *widget;

	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	fs = gtk_type_new (EOG_TYPE_FULL_SCREEN);

	fs->priv->image_view = eog_image_view_new (image, TRUE);
	widget = eog_image_view_get_widget (fs->priv->image_view);
	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (fs), widget);

	return (GTK_WIDGET (fs));
}

