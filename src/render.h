/* Eye of Gnome image viewer - core rendering module
 *
 * Copyright (C) 2000 The Free Software Foundation
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

#ifndef RENDER_H
#define RENDER_H

#include <gdk-pixbuf/gdk-pixbuf.h>



void render_image (GdkPixbuf *pixbuf, guchar *dest,
		   guint dest_width, guint dest_height, guint dest_rowstride,
		   double scale_width, double scale_height, guint xofs, guint yofs,
		   guchar *r_lut, guchar *g_lut, guchar *b_lut,
		   guchar dark_check, guchar light_check);



#endif
