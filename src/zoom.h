/* Eye of Gnome image viewer - utility functions for computing zoom factors
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnome.org>
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

#include <glib.h>

G_GNUC_INTERNAL
void zoom_fit_size (guint dest_width, guint dest_height,
		    guint src_width, guint src_height,
		    gboolean upscale_smaller,
		    guint *width, guint *height);

G_GNUC_INTERNAL
double zoom_fit_scale (guint dest_width, guint dest_height,
		       guint src_width, guint src_height,
		       gboolean upscale_smaller);
