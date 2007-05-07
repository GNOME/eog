/* Eye Of Gnome - EOG Plugin Engine 
 *
 * Copyright (C) 2007 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gedit code (gedit/gedit-plugins-engine.h) by: 
 * 	- Paolo Maggi <paolo@gnome.org>
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

#ifndef __EOG_PLUGIN_ENGINE_H__
#define __EOG_PLUGIN_ENGINE_H__

#include "eog-window.h"

#include <glib.h>

typedef struct _EogPluginInfo EogPluginInfo;

gboolean	 eog_plugin_engine_init 		(void);

void		 eog_plugin_engine_shutdown 		(void);

void		 eog_plugin_engine_garbage_collect	(void);

const GList	*eog_plugin_engine_get_plugins_list 	(void);

gboolean 	 eog_plugin_engine_activate_plugin 	(EogPluginInfo *info);

gboolean 	 eog_plugin_engine_deactivate_plugin	(EogPluginInfo *info);

gboolean 	 eog_plugin_engine_plugin_is_active 	(EogPluginInfo *info);

gboolean 	 eog_plugin_engine_plugin_is_available	(EogPluginInfo *info);

gboolean	 eog_plugin_engine_plugin_is_configurable 
			       				(EogPluginInfo *info);

void	 	 eog_plugin_engine_configure_plugin	(EogPluginInfo *info, 
			       			 	 GtkWindow     *parent);

void		 eog_plugin_engine_update_plugins_ui	(EogWindow     *window, 
			       			 	 gboolean       new_window);

const gchar	*eog_plugin_engine_get_plugin_name	(EogPluginInfo *info);

const gchar	*eog_plugin_engine_get_plugin_description
			       				(EogPluginInfo *info);

const gchar	*eog_plugin_engine_get_plugin_icon_name (EogPluginInfo *info);

const gchar    **eog_plugin_engine_get_plugin_authors   (EogPluginInfo *info);

const gchar	*eog_plugin_engine_get_plugin_website   (EogPluginInfo *info);

const gchar	*eog_plugin_engine_get_plugin_copyright (EogPluginInfo *info);

#endif  /* __EOG_PLUGIN_ENGINE_H__ */
