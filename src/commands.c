/* Eye of Gnome image viewer - commands selectable from the menus and toolbars
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
#include "commands.h"
#include "full-screen.h"
#include "image-view.h"
#include "ui-image.h"
#include "window.h"



/* Sets the zoom factor of a window's image view */
static void
set_window_zoom (Window *window, double zoom, gboolean mult)
{
	UIImage *ui;
	ImageView *view;

	ui = UI_IMAGE (window_get_ui_image (window));
	view = IMAGE_VIEW (ui_image_get_image_view (ui));

	if (mult)
		zoom *= image_view_get_zoom (view);

	image_view_set_zoom (view, zoom);
}

void
cmd_cb_image_open (GtkWidget *widget, gpointer data)
{
	window_open_image_dialog (WINDOW (data));
}

void
cmd_cb_window_close (GtkWidget *widget, gpointer data)
{
	window_close (WINDOW (data));
}

void
cmd_cb_zoom_in (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.05, TRUE);
}

void
cmd_cb_zoom_out (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 1.05, TRUE);
}

void
cmd_cb_zoom_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0, FALSE);
}

void
cmd_cb_zoom_fit (GtkWidget *widget, gpointer data)
{
	UIImage *ui;

	ui = UI_IMAGE (window_get_ui_image (WINDOW (data)));
	ui_image_zoom_fit (ui);
}

void
cmd_cb_full_screen (GtkWidget *widget, gpointer data)
{
	UIImage *ui;
	ImageView *view;
	GtkWidget *fs;

	ui = UI_IMAGE (window_get_ui_image (WINDOW (data)));
	view = IMAGE_VIEW (ui_image_get_image_view (ui));

	fs = full_screen_new ();
	full_screen_set_image (FULL_SCREEN (fs), image_view_get_image (view));
	gtk_widget_show (fs);
}



void
cmd_cb_zoom_2_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 2.0, FALSE);
}

void
cmd_cb_zoom_3_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 3.0, FALSE);
}

void
cmd_cb_zoom_4_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 4.0, FALSE);
}

void
cmd_cb_zoom_5_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 5.0, FALSE);
}

void
cmd_cb_zoom_6_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 6.0, FALSE);
}

void
cmd_cb_zoom_7_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 7.0, FALSE);
}

void
cmd_cb_zoom_8_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 8.0, FALSE);
}

void
cmd_cb_zoom_9_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 9.0, FALSE);
}

void
cmd_cb_zoom_10_1 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 10.0, FALSE);
}



void
cmd_cb_zoom_1_2 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 2.0, FALSE);
}

void
cmd_cb_zoom_1_3 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 3.0, FALSE);
}

void
cmd_cb_zoom_1_4 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 4.0, FALSE);
}

void
cmd_cb_zoom_1_5 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 5.0, FALSE);
}

void
cmd_cb_zoom_1_6 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 6.0, FALSE);
}

void
cmd_cb_zoom_1_7 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 7.0, FALSE);
}

void
cmd_cb_zoom_1_8 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 8.0, FALSE);
}

void
cmd_cb_zoom_1_9 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 9.0, FALSE);
}

void
cmd_cb_zoom_1_10 (GtkWidget *widget, gpointer data)
{
	set_window_zoom (WINDOW (data), 1.0 / 10.0, FALSE);
}
