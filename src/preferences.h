/* Eye of Gnome image viewer - preferences
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gnu.org>
 *          Arik Devens <arik@gnome.org>
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkenums.h>

/* Type of checks for views */
typedef enum {
	CHECK_TYPE_DARK,
	CHECK_TYPE_MIDTONE,
	CHECK_TYPE_LIGHT,
	CHECK_TYPE_BLACK,
	CHECK_TYPE_GRAY,
	CHECK_TYPE_WHITE
} CheckType;

/* Check size for views */
typedef enum {
	CHECK_SIZE_SMALL,
	CHECK_SIZE_MEDIUM,
	CHECK_SIZE_LARGE
} CheckSize;

/* Scrolling type for views */
typedef enum {
	SCROLL_NORMAL,
	SCROLL_TWO_PASS
} ScrollType;

/* Automatic zoom for full screen mode */
typedef enum {
	FULL_SCREEN_ZOOM_1,
	FULL_SCREEN_ZOOM_SAME_AS_WINDOW,
	FULL_SCREEN_ZOOM_FIT
} FullScreenZoom;


void prefs_init (void);

void prefs_dialog (void);

#endif
