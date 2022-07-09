/* Eye Of Gnome - Session Handler
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gedit code (gedit/gedit-session.h) by:
 * 	- Gedit Team
 * 	- Federico Mena-Quintero <federico@ximian.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include "eog-application.h"

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
void 		eog_session_init 		(EogApplication *application);

G_GNUC_INTERNAL
gboolean 	eog_session_is_restored 	(void);

G_GNUC_INTERNAL
gboolean 	eog_session_load 		(void);

G_END_DECLS
