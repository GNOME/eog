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

#include <config.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-config.h>
#include "preferences.h"



GdkInterpType  prefs_interp_type;
CheckType      prefs_check_type;
CheckSize      prefs_check_size;
GdkRgbDither   prefs_dither;
ScrollType     prefs_scroll;
GtkPolicyType  prefs_window_sb_policy;
gboolean       prefs_window_auto_size;
gboolean       prefs_open_new_window;
GtkPolicyType  prefs_full_screen_sb_policy;
FullScreenZoom prefs_full_screen_zoom;
gboolean       prefs_full_screen_fit_standard;
gboolean       prefs_full_screen_bevel;



/**
 * prefs_init:
 * @void: 
 * 
 * Reads the default set of preferences.  Must be called prior to any of the
 * rest of the program running.
 **/
void
prefs_init (void)
{
	gnome_config_push_prefix ("/eog/");

	prefs_interp_type = gnome_config_get_int (
		"view/interp_type=2"); /* bilinear */
	prefs_check_type = gnome_config_get_int (
		"view/check_type=1"); /* midtone */
	prefs_check_size = gnome_config_get_int (
		"view/check_size=2"); /* large */
	prefs_dither = gnome_config_get_int (
		"view/dither=1"); /* normal */
	prefs_scroll = gnome_config_get_int (
		"view/scroll=1"); /* two-pass */

	prefs_window_sb_policy = gnome_config_get_int (
		"window/sb_policy=1"); /* automatic */
	prefs_window_auto_size = gnome_config_get_bool (
		"window/auto_size=1"); /* true */
	prefs_open_new_window = gnome_config_get_bool (
		"window/open_new_window=0"); /* false */

	prefs_full_screen_sb_policy = gnome_config_get_int (
		"full_screen/sb_policy=1"); /* never */
	prefs_full_screen_zoom = gnome_config_get_int (
		"full_screen/zoom=2"); /* fit */
	prefs_full_screen_fit_standard = gnome_config_get_bool (
		"full_screen/fit_standard=1"); /* true */
	prefs_full_screen_bevel = gnome_config_get_bool (
		"full_screen/bevel=0"); /* false */

	gnome_config_pop_prefix ();
}
