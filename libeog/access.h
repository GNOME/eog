/* Eye of Gnome image viewer - Utility functions for accessibility 
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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

#ifndef ACCESS_H
#define ACCESS_H

#include <atk/atk.h>
#include <gtk/gtkwidget.h>

void access_add_atk_relation (GtkWidget *widget1, GtkWidget *widget2,
			      AtkRelationType w1_to_w2, AtkRelationType w2_to_w1);

#endif
