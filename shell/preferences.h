/* Eye of Gnome image viewer - preferences
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkenums.h>



/* Interpolation type for views */
extern GdkInterpType prefs_interp_type;

/* Type of checks for views */
typedef enum {
	CHECK_TYPE_DARK,
	CHECK_TYPE_MIDTONE,
	CHECK_TYPE_LIGHT,
	CHECK_TYPE_BLACK,
	CHECK_TYPE_GRAY,
	CHECK_TYPE_WHITE
} CheckType;

extern CheckType prefs_check_type;

/* Check size for views */
typedef enum {
	CHECK_SIZE_SMALL,
	CHECK_SIZE_MEDIUM,
	CHECK_SIZE_LARGE
} CheckSize;

extern CheckSize prefs_check_size;

/* Dither type */
extern GdkRgbDither prefs_dither;

/* Scrolling type for views */
typedef enum {
	SCROLL_NORMAL,
	SCROLL_TWO_PASS
} ScrollType;

extern ScrollType prefs_scroll;

/* Scrollbar policy for image windows */
extern GtkPolicyType prefs_window_sb_policy;

/* Pick window size and zoom factor automatically for image windows */
extern gboolean prefs_window_auto_size;

/* Open images in a new window */
extern gboolean prefs_open_new_window;

/* Scrollbar policy for full screen mode */
extern GtkPolicyType prefs_full_screen_sb_policy;

/* Automatic zoom for full screen mode */
typedef enum {
	FULL_SCREEN_ZOOM_1,
	FULL_SCREEN_ZOOM_SAME_AS_WINDOW,
	FULL_SCREEN_ZOOM_FIT
} FullScreenZoom;

extern FullScreenZoom prefs_full_screen_zoom;

/* Fit standard-sized images to full screen mode */
extern gboolean prefs_full_screen_fit_standard;

/* Put a bevel around the edge of the screen */
extern gboolean prefs_full_screen_bevel;



void prefs_init (void);

void prefs_dialog (void);



#endif
