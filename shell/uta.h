/* Eye of Gnome image viewer - Microtile array utilities
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

#ifndef UTA_H
#define UTA_H

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_uta.h>



ArtUta *uta_ensure_size (ArtUta *uta, int x1, int y1, int x2, int y2);

ArtUta *uta_add_rect (ArtUta *uta, int x1, int y1, int x2, int y2);
void uta_remove_rect (ArtUta *uta, int x1, int y1, int x2, int y2);

void uta_find_first_glom_rect (ArtUta *uta, ArtIRect *rect, int max_width, int max_height);

void uta_copy_area (ArtUta *uta, int src_x, int src_y, int dest_x, int dest_y, int width, int height);



#endif
