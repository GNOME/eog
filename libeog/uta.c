/* Eye of Gnome image viewer - Microtile array utilities
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
#include <glib.h>
#include "uta.h"
#include <libart_lgpl/art_uta_rect.h>



/**
 * uta_ensure_size:
 * @uta: A microtile array.
 * @x1: Left microtile coordinate that must fit in new array.
 * @y1: Top microtile coordinate that must fit in new array.
 * @x2: Right microtile coordinate that must fit in new array.
 * @y2: Bottom microtile coordinate that must fit in new array.
 * 
 * Ensures that the size of a microtile array is big enough to fit the specified
 * microtile coordinates.  If it is not big enough, the specified @uta will be
 * freed and a new one will be returned.  Otherwise, the original @uta will be
 * returned.  If a new microtile array needs to be created, this function will
 * copy the @uta's contents to the new array.
 *
 * Note that the specified coordinates must have already been scaled down by the
 * ART_UTILE_SHIFT factor.
 * 
 * Return value: The same value as @uta if the original microtile array was
 * big enough to fit the specified microtile coordinates, or a new array if
 * it needed to be grown.  In the second case, the original @uta will be
 * freed automatically.
 **/
ArtUta *
uta_ensure_size (ArtUta *uta, int x1, int y1, int x2, int y2)
{
	ArtUta *new_uta;
	ArtUtaBbox *utiles, *new_utiles;
	int new_ofs, ofs;
	int x, y;

	g_return_val_if_fail (uta != NULL, NULL);
	g_return_val_if_fail (x1 < x2, NULL);
	g_return_val_if_fail (y1 < y2, NULL);

	if (x1 >= uta->x0
	    && y1 >= uta->y0
	    && x2 <= uta->x0 + uta->width
	    && y2 <= uta->y0 + uta->height)
		return uta;

	new_uta = art_new (ArtUta, 1);

	new_uta->x0 = MIN (uta->x0, x1);
	new_uta->y0 = MIN (uta->y0, y1);
	new_uta->width = MAX (uta->x0 + uta->width, x2) - new_uta->x0;
	new_uta->height = MAX (uta->y0 + uta->height, y2) - new_uta->y0;
	new_uta->utiles = art_new (ArtUtaBbox, new_uta->width * new_uta->height);

	utiles = uta->utiles;
	new_utiles = new_uta->utiles;

	new_ofs = 0;

	for (y = new_uta->y0; y < new_uta->y0 + new_uta->height; y++) {
		if (y < uta->y0 || y >= uta->y0 + uta->height)
			for (x = 0; x < new_uta->width; x++)
				new_utiles[new_ofs++] = 0;
		else {
			ofs = (y - uta->y0) * uta->width;

			for (x = new_uta->x0; x < new_uta->x0 + new_uta->width; x++)
				if (x < uta->x0 || x >= uta->x0 + uta->width)
					new_utiles[new_ofs++] = 0;
				else
					new_utiles[new_ofs++] = utiles[ofs++];
		}
	}

	art_uta_free (uta);
	return new_uta;
}

/**
 * uta_add_rect:
 * @uta: A microtile array, or NULL if a new array should be created.
 * @x1: Left coordinate of rectangle.
 * @y1: Top coordinate of rectangle.
 * @x2: Right coordinate of rectangle.
 * @y2: Bottom coordinate of rectangle.
 * 
 * Adds the specified rectangle to a microtile array.  The array is
 * grown to fit the rectangle if necessary.
 * 
 * Return value: The original @uta, or a new microtile array if the original one
 * needed to be grown to fit the specified rectangle.  In the second case, the
 * original @uta will be freed automatically.
 **/
ArtUta *
uta_add_rect (ArtUta *uta, int x1, int y1, int x2, int y2)
{
	ArtUtaBbox *utiles;
	ArtUtaBbox bb;
	int rect_x1, rect_y1, rect_x2, rect_y2;
	int x, y;
	int ofs;
	int xf1, yf1, xf2, yf2;

	g_return_val_if_fail (x1 < x2, NULL);
	g_return_val_if_fail (y1 < y2, NULL);

	/* Empty uta */

	if (!uta) {
		ArtIRect r;

		r.x0 = x1;
		r.y0 = y1;
		r.x1 = x2;
		r.y1 = y2;

		return art_uta_from_irect (&r);
	}

	/* Grow the uta if necessary */

	rect_x1 = x1 >> ART_UTILE_SHIFT;
	rect_y1 = y1 >> ART_UTILE_SHIFT;
	rect_x2 = (x2 >> ART_UTILE_SHIFT) + 1;
	rect_y2 = (y2 >> ART_UTILE_SHIFT) + 1;

	uta = uta_ensure_size (uta, rect_x1, rect_y1, rect_x2, rect_y2);

	/* Add the rectangle */

	xf1 = x1 & (ART_UTILE_SIZE - 1);
	yf1 = y1 & (ART_UTILE_SIZE - 1);
	xf2 = ((x2 - 1) & (ART_UTILE_SIZE - 1)) + 1;
	yf2 = ((y2 - 1) & (ART_UTILE_SIZE - 1)) + 1;

	utiles = uta->utiles;

	ofs = (rect_y1 - uta->y0) * uta->width + rect_x1 - uta->x0;

	if (rect_y2 - rect_y1 == 1) {
		if (rect_x2 - rect_x1 == 1) {
			bb = utiles[ofs];
			utiles[ofs] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
							 MIN (ART_UTA_BBOX_Y0 (bb), yf1),
							 MAX (ART_UTA_BBOX_X1 (bb), xf2),
							 MAX (ART_UTA_BBOX_Y1 (bb), yf2));
		} else {
			/* Leftmost tile */
			bb = utiles[ofs];
			utiles[ofs++] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
							   MIN (ART_UTA_BBOX_Y0 (bb), yf1),
							   ART_UTILE_SIZE,
							   MAX (ART_UTA_BBOX_Y1 (bb), yf2));

			/* Tiles in between */
			for (x = rect_x1 + 1; x < rect_x2; x++) {
				bb = utiles[ofs];
				utiles[ofs++] = ART_UTA_BBOX_CONS (0,
								   MIN (ART_UTA_BBOX_Y0 (bb), yf1),
								   ART_UTILE_SIZE,
								   MAX (ART_UTA_BBOX_Y1 (bb), yf2));
			}

			/* Rightmost tile */
			bb = utiles[ofs];
			utiles[ofs] = ART_UTA_BBOX_CONS (0,
							 MIN (ART_UTA_BBOX_Y0 (bb), yf1),
							 MAX (ART_UTA_BBOX_X1 (bb), xf2),
							 MAX (ART_UTA_BBOX_Y1 (bb), yf2));
		}
	} else {
		if (rect_x2 - rect_x1 == 1) {
			/* Topmost tile */
			bb = utiles[ofs];
			utiles[ofs] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
							 MIN (ART_UTA_BBOX_Y0 (bb), yf1),
							 MAX (ART_UTA_BBOX_X1 (bb), xf2),
							 ART_UTILE_SIZE);
			ofs += uta->width;

			/* Tiles in between */
			for (y = rect_y1 + 1; y < rect_y2; y++) {
				bb = utiles[ofs];
				utiles[ofs] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
								 0,
								 MAX (ART_UTA_BBOX_X1 (bb), xf2),
								 ART_UTILE_SIZE);
				ofs += uta->width;
			}

			/* Bottommost tile */
			bb = utiles[ofs];
			utiles[ofs] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
							 0,
							 MAX (ART_UTA_BBOX_X1 (bb), xf2),
							 MAX (ART_UTA_BBOX_Y1 (bb), yf2));
		} else {
			/* Top row, leftmost tile */
			bb = utiles[ofs];
			utiles[ofs++] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
							   MIN (ART_UTA_BBOX_Y0 (bb), yf1),
							   ART_UTILE_SIZE,
							   ART_UTILE_SIZE);

			/* Top row, in between */
			for (x = rect_x1 + 1; x < rect_x2; x++) {
				bb = utiles[ofs];
				utiles[ofs++] = ART_UTA_BBOX_CONS (0,
								   MIN (ART_UTA_BBOX_Y0 (bb), yf1),
								   ART_UTILE_SIZE,
								   ART_UTILE_SIZE);
			}

			/* Top row, rightmost tile */ 
			bb = utiles[ofs];
			utiles[ofs] = ART_UTA_BBOX_CONS (0,
							 MIN (ART_UTA_BBOX_Y0 (bb), yf1),
							 MAX (ART_UTA_BBOX_X1 (bb), xf2),
							 ART_UTILE_SIZE);

			ofs += uta->width - (rect_x2 - rect_x1);

			/* Rows in between */
			for (y = rect_y1 + 1; y < rect_x2; y++) {
				/* Leftmost tile */
				bb = utiles[ofs];
				utiles[ofs++] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
								   0,
								   ART_UTILE_SIZE,
								   ART_UTILE_SIZE);

				/* Tiles in between */
				bb = ART_UTA_BBOX_CONS (0, 0, ART_UTILE_SIZE, ART_UTILE_SIZE);
				for (x = rect_x1 + 1; x < rect_x2; x++)
					utiles[ofs++] = bb;

				/* Rightmost tile */
				bb = utiles[ofs];
				utiles[ofs++] = ART_UTA_BBOX_CONS (0,
								   0,
								   MAX (ART_UTA_BBOX_X1 (bb), xf2),
								   ART_UTILE_SIZE);

				ofs += uta->width - (rect_x2 - rect_x1);
			}

			/* Bottom row, leftmost tile */
			bb = utiles[ofs];
			utiles[ofs++] = ART_UTA_BBOX_CONS (MIN (ART_UTA_BBOX_X0 (bb), xf1),
							   0,
							   ART_UTILE_SIZE,
							   MAX (ART_UTA_BBOX_Y1 (bb), yf2));

			/* Bottom row, tiles in between */
			for (x = rect_x1 + 1; x < rect_x2; x++) {
				bb = utiles[ofs];
				utiles[ofs++] = ART_UTA_BBOX_CONS (0,
								   0,
								   ART_UTILE_SIZE,
								   MAX (ART_UTA_BBOX_Y1 (bb), yf2));
			}

			/* Bottom row, rightmost tile */
			bb = utiles[ofs];
			utiles[ofs] = ART_UTA_BBOX_CONS (0,
							 0,
							 MAX (ART_UTA_BBOX_X1 (bb), xf2),
							 MAX (ART_UTA_BBOX_Y1 (bb), yf2));
		}
	}

	return uta;
}

void
uta_remove_rect (ArtUta *uta, int x1, int y1, int x2, int y2)
{
	ArtUtaBbox *utiles;
	ArtUtaBbox bb;
	int rect_x1, rect_y1, rect_x2, rect_y2;
	int clip_x1, clip_y1, clip_x2, clip_y2;
	int ofs;
	int xf1, yf1, xf2, yf2;

	g_return_if_fail (x1 < x2);
	g_return_if_fail (y1 < y2);

	rect_x1 = x1 >> ART_UTILE_SHIFT;
	rect_y1 = y1 >> ART_UTILE_SHIFT;
	rect_x2 = (x2 >> ART_UTILE_SHIFT) + 1;
	rect_y2 = (y2 >> ART_UTILE_SHIFT) + 1;

	clip_x1 = MAX (rect_x1, uta->x0);
	clip_y1 = MAX (rect_y1, uta->y0);
	clip_x2 = MAX (rect_x2, uta->x0 + uta->width);
	clip_y2 = MAX (rect_y2, uta->y0 + uta->height);

	if (clip_x1 >= clip_x2 || clip_y1 >= clip_y2)
		return;

	xf1 = x1 & (ART_UTILE_SIZE - 1);
	yf1 = y1 & (ART_UTILE_SIZE - 1);
	xf2 = ((x2 - 1) & (ART_UTILE_SIZE - 1)) + 1;
	yf2 = ((y2 - 1) & (ART_UTILE_SIZE - 1)) + 1;

	utiles = uta->utiles;

	/* Top row */
	if (rect_y1 >= uta->y0 && rect_y1 < uta->y0 + uta->height) {
		ofs = (rect_y1 - uta->y0) * uta->width;

		/* Leftmost tile */
		if (rect_x1 >= uta->x0 && rect_x1 < uta->x0 + uta->width) {
			ofs += rect_x1 - uta->x0;

			bb = utiles[ofs];
			
		}
	}
}

void
uta_find_first_glom_rect (ArtUta *uta, ArtIRect *rect, int max_width, int max_height)
{
	/* FIXME */
}
