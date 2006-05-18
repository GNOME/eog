/* Eye Of Gnome - GConf Keys Macros 
 *
 * Copyright (C) 2000-2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on code by:
 * 	- Federico Mena-Quintero <federico@gnu.org>
 *	- Jens Finke <jens@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EOG_CONFIG_KEYS_H__
#define __EOG_CONFIG_KEYS_H__

#define EOG_CONF_DIR			"/apps/eog"

#define EOG_CONF_DESKTOP_CAN_SAVE	"/desktop/gnome/lockdown/disable_save_to_disk"

#define EOG_CONF_VIEW_INTERPOLATE	"/apps/eog/view/interpolate"
#define EOG_CONF_VIEW_TRANSPARENCY	"/apps/eog/view/transparency"
#define EOG_CONF_VIEW_TRANS_COLOR	"/apps/eog/view/trans_color"

#define EOG_CONF_WINDOW_GEOMETRY	"/apps/eog/window/geometry_singleton"

#define EOG_CONF_FULLSCREEN_LOOP	"/apps/eog/full_screen/loop"
#define EOG_CONF_FULLSCREEN_UPSCALE	"/apps/eog/full_screen/upscale"
#define EOG_CONF_FULLSCREEN_SECONDS	"/apps/eog/full_screen/seconds"

#define EOG_CONF_UI_TOOLBAR		"/apps/eog/ui/toolbar"
#define EOG_CONF_UI_STATUSBAR		"/apps/eog/ui/statusbar"
#define EOG_CONF_UI_IMAGE_COLLECTION	"/apps/eog/ui/image_collection"

#endif /* __EOG_CONFIG_KEYS_H__ */
