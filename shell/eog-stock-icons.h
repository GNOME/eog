/* Eye Of Gnome - Stock Icons Handling 
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on evince code (shell/ev-stock-icons.h) by: 
 * 	- Martin Kretzschmar <martink@gnome.org>
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

#ifndef __EOG_STOCK_ICONS_H__
#define __EOG_STOCK_ICONS_H__

#include <glib/gmacros.h>

G_BEGIN_DECLS

#define EOG_STOCK_ROTATE_90       "stock-rotate-90"
#define EOG_STOCK_ROTATE_270      "stock-rotate-270"
#define EOG_STOCK_ROTATE_180      "stock-rotate-180"
#define EOG_STOCK_FLIP_HORIZONTAL "stock-flip-horizontal"
#define EOG_STOCK_FLIP_VERTICAL   "stock-flip-vertical"

void eog_stock_icons_init (void);

G_END_DECLS

#endif /* __EOG_STOCK_ICONS_H__ */
