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

#define EOG_CONF_DOMAIN				"org.gnome.eog"
#define EOG_CONF_FULLSCREEN			EOG_CONF_DOMAIN".full_screen"
#define EOG_CONF_PLUGINS			EOG_CONF_DOMAIN".plugins"
#define EOG_CONF_UI				EOG_CONF_DOMAIN".ui"
#define EOG_CONF_VIEW				EOG_CONF_DOMAIN".view"

#define EOG_CONF_DESKTOP_WALLPAPER		"/desktop/gnome/background/picture_filename"
#define EOG_CONF_DESKTOP_CAN_SAVE		"/desktop/gnome/lockdown/disable_save_to_disk"
#define EOG_CONF_DESKTOP_CAN_PRINT      	"/desktop/gnome/lockdown/disable_printing"
#define EOG_CONF_DESKTOP_CAN_SETUP_PAGE 	"/desktop/gnome/lockdown/disable_print_setup"

#define EOG_CONF_VIEW_INTERPOLATE		"interpolate"
#define EOG_CONF_VIEW_EXTRAPOLATE		"extrapolate"
#define EOG_CONF_VIEW_SCROLL_WHEEL_ZOOM		"scroll_wheel_zoom"
#define EOG_CONF_VIEW_ZOOM_MULTIPLIER		"zoom_multiplier"
#define EOG_CONF_VIEW_AUTOROTATE                "autorotate"
#define EOG_CONF_VIEW_TRANSPARENCY		"transparency"
#define EOG_CONF_VIEW_TRANS_COLOR		"trans_color"

#define EOG_CONF_FULLSCREEN_LOOP		"loop"
#define EOG_CONF_FULLSCREEN_UPSCALE		"upscale"
#define EOG_CONF_FULLSCREEN_SECONDS		"seconds"

#define EOG_CONF_UI_TOOLBAR			"toolbar"
#define EOG_CONF_UI_STATUSBAR			"statusbar"
#define EOG_CONF_UI_IMAGE_COLLECTION		"image_collection"
#define EOG_CONF_UI_IMAGE_COLLECTION_POSITION	"image_collection_position"
#define EOG_CONF_UI_IMAGE_COLLECTION_RESIZABLE	"image_collection_resizable"
#define EOG_CONF_UI_SIDEBAR			"sidebar"
#define EOG_CONF_UI_SCROLL_BUTTONS		"scroll_buttons"
#define EOG_CONF_UI_DISABLE_TRASH_CONFIRMATION	"disable_trash_confirmation"
#define EOG_CONF_UI_FILECHOOSER_XDG_FALLBACK	"filechooser_xdg_fallback"
#define EOG_CONF_UI_PROPSDIALOG_NETBOOK_MODE	"propsdialog_netbook_mode"

#define EOG_CONF_PLUGINS_ACTIVE_PLUGINS         "active_plugins"

#endif /* __EOG_CONFIG_KEYS_H__ */
