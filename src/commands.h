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

#ifndef COMMANDS_H
#define COMMANDS_H

#include <gtk/gtkwidget.h>



/* Callbacks for menu items and toolbar buttons.  They all take in a Window
 * argument in the data.
 */

void cmd_cb_image_open (GtkWidget *widget, gpointer data);
void cmd_cb_window_close (GtkWidget *widget, gpointer data);

void cmd_cb_zoom_in (GtkWidget *widget, gpointer data);
void cmd_cb_zoom_out (GtkWidget *widget, gpointer data);
void cmd_cb_zoom_1 (GtkWidget *widget, gpointer data);
void cmd_cb_zoom_fit (GtkWidget *widget, gpointer data);



#endif
