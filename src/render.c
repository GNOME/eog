/* Eye of Gnome image viewer - core rendering module
 *
 * Copyright (C) 1999 The Free Software Foundation
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
#include <math.h>
#include "render.h"



/* Size of checks */
#define CHECK_MASK (1 << 4)



/* Computes a lookup table of offsets for rendering based on the zoom factor */
static int *
compute_coord_lut (guint size, double zoom, guint ofs)
{
	int *lut;
	int i;

	lut = g_new (int, size);

	for (i = 0; i < size; i++)
		lut[i] = floor ((ofs + i + 0.5) / zoom);

	return lut;
}



/* Renders a portion of an RGB image to a buffer */
static void
render_rgb (ArtPixBuf *pixbuf, guchar *dest,
	    guint dest_width, guint dest_height, guint dest_rowstride,
	    int *xlut, int *ylut,
	    guchar *r_lut, guchar *g_lut, guchar *b_lut)
{
	int x, y;
	guchar *src_row, *s;
	guchar *p;

	g_assert (pixbuf->format == ART_PIX_RGB);
	g_assert (pixbuf->n_channels == 3);
	g_assert (!pixbuf->has_alpha);
	g_assert (pixbuf->bits_per_sample == 8);

	for (y = 0; y < dest_height; y++) {
		src_row = pixbuf->pixels + ylut[y];
		p = dest + y * dest_rowstride;

		for (x = 0; x < dest_width; x++) {
			s = src_row + xlut[x];

			*p++ = r_lut[*s++];
			*p++ = g_lut[*s++];
			*p++ = b_lut[*s];
		}
	}
}

/* Renders a portion of an RGBA image to a buffer */
static void
render_rgba (ArtPixBuf *pixbuf, guchar *dest,
	     guint dest_width, guint dest_height, guint dest_rowstride,
	     guint xofs, guint yofs, int *xlut, int *ylut,
	     guchar *r_lut, guchar *g_lut, guchar *b_lut,
	     guchar dark_check, guchar light_check)
{
	int x, y;
	guchar *src_row, *s;
	guchar *p;
	int dark_x, dark_y;
	int alpha;
	guchar check;

	g_assert (pixbuf->format == ART_PIX_RGB);
	g_assert (pixbuf->n_channels == 4);
	g_assert (pixbuf->has_alpha);
	g_assert (pixbuf->bits_per_sample == 8);

	for (y = 0; y < dest_height; y++) {
		src_row = pixbuf->pixels + ylut[y];
		p = dest + y * dest_rowstride;
		dark_y = ((y + yofs) & CHECK_MASK) != 0;

		for (x = 0; x < dest_width; x++) {
			s = src_row + xlut[x];
			dark_x = ((x + xofs) & CHECK_MASK) != 0;

			if (dark_x ^ dark_y)
				check = dark_check;
			else
				check = light_check;

			alpha = s[3];

			if (alpha == 0) {
				*p++ = check;
				*p++ = check;
				*p++ = check;
			} else if (alpha == 255) {
				*p++ = *s++;
				*p++ = *s++;
				*p++ = *s;
			} else {
				*p++ = check + (((r_lut[*s++] - check) * alpha + 0x80) >> 8);
				*p++ = check + (((g_lut[*s++] - check) * alpha + 0x80) >> 8);
				*p++ = check + (((b_lut[*s] - check) * alpha + 0x80) >> 8);
			}
		}
	}
}



/**
 * render_image:
 * @image: An image.
 * @dest: Destination RGB buffer.
 * @dest_width: Width of destination.
 * @dest_height: Height of destination.
 * @dest_rowstride: Rowstride of destination.
 * @zoom: Zoom factor for rendering.
 * @xofs: Horizontal rendering offset.
 * @yofs: Vertical rendering offset.
 * @r_lut: Color lookup table for the red channel.
 * @g_lut: Color lookup table for the green channel.
 * @b_lut: Color lookup table for the blue channel.
 * @dark_check: Intensity of dark checks.
 * @light_check: Intensity of light checks.
 * 
 * Renders the specified @image to the specified @dest buffer.  The specified
 * offsets and dimensions must define a rectangle that fit inside the image when
 * zoomed.
 **/
void
render_image (Image *image, guchar *dest,
	      guint dest_width, guint dest_height, guint dest_rowstride,
	      double zoom, guint xofs, guint yofs,
	      guchar *r_lut, guchar *g_lut, guchar *b_lut,
	      guchar dark_check, guchar light_check)
{
	ArtPixBuf *pixbuf;
	int xzoom_size, yzoom_size;
	double xzoom_adj, yzoom_adj;
	int *xlut, *ylut;
	int i;

	pixbuf = image->buf->art_pixbuf;

	/* Sanity checks */

	g_return_if_fail (image != NULL);
	g_return_if_fail (pixbuf->format == ART_PIX_RGB);
	g_return_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4);
	g_return_if_fail (pixbuf->bits_per_sample == 8);

	g_return_if_fail (dest != NULL);
	g_return_if_fail (dest_width > 0);
	g_return_if_fail (dest_height > 0);
	g_return_if_fail (dest_rowstride > 0);
	g_return_if_fail (zoom > 0.0);

	xzoom_size = floor (pixbuf->width * zoom + 0.5);
	yzoom_size = floor (pixbuf->height * zoom + 0.5);
	g_return_if_fail (xofs < xzoom_size);
	g_return_if_fail (yofs < yzoom_size);
	g_return_if_fail (xofs + dest_width <= xzoom_size);
	g_return_if_fail (yofs + dest_height <= yzoom_size);

	g_return_if_fail (r_lut != NULL);
	g_return_if_fail (g_lut != NULL);
	g_return_if_fail (b_lut != NULL);

	/* Build rendering coordinate LUTs */

	xzoom_adj = (double) xzoom_size  / pixbuf->width;
	xlut = compute_coord_lut (dest_width, xzoom_adj, xofs);
	for (i = 0; i < dest_width; i++)
		xlut[i] *= pixbuf->n_channels;

	yzoom_adj = (double) yzoom_size  / pixbuf->height;
	ylut = compute_coord_lut (dest_height, yzoom_adj, yofs);
	for (i = 0; i < dest_height; i++)
		ylut[i] *= pixbuf->rowstride;

	/* Render! */

	if (pixbuf->has_alpha)
		render_rgba (pixbuf, dest, dest_width, dest_height, dest_rowstride,
			     xofs, yofs, xlut, ylut,
			     r_lut, g_lut, b_lut,
			     dark_check, light_check);
	else
		render_rgb (pixbuf, dest, dest_width, dest_height, dest_rowstride,
			    xlut, ylut,
			    r_lut, g_lut, b_lut);

	g_free (xlut);
	g_free (ylut);
}
