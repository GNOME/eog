/* Eye of Gnome image viewer - image structure and image loading
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

#include <config.h>
#include "image.h"



/**
 * image_new:
 * @void:
 *
 * Creates a new empty image structure with a reference count of 1.
 *
 * Return value: A newly-created image structure.
 **/
Image *
image_new (void)
{
	Image *image;
	int i;

	image = g_new0 (Image, 1);
	image->ref_count = 1;

	/* Create the default lookup tables */

	image->r_lut = g_new (guchar, 256);
	image->g_lut = g_new (guchar, 256);
	image->b_lut = g_new (guchar, 256);

	for (i = 0; i < 256; i++) {
		image->r_lut[i] = i;
		image->g_lut[i] = i;
		image->b_lut[i] = i;
	}

	return image;
}

/**
 * image_ref:
 * @image: An image structure.
 *
 * Adds a reference to an image.
 **/
void
image_ref (Image *image)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (image->ref_count > 0);

	image->ref_count++;
}

/**
 * image_unref:
 * @image: An image structure.
 *
 * Removes a reference from an image.  If the reference count drops to zero,
 * then the image is freed.
 **/
void
image_unref (Image *image)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (image->ref_count > 0);

	image->ref_count--;

	if (image->ref_count == 0) {
		if (image->pixbuf)
			gdk_pixbuf_unref (image->pixbuf);

		g_free (image->r_lut);
		g_free (image->g_lut);
		g_free (image->b_lut);

		if (image->filename)
			g_free (image->filename);

		g_free (image);
	}
}

/**
 * image_load:
 * @image: An image structure.
 * @filename: Name of file with image data.
 *
 * Loads an image from the specified file.
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
image_load (Image *image, const char *filename)
{
	g_return_val_if_fail (image != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (image->pixbuf)
		gdk_pixbuf_unref (image->pixbuf);

	image->pixbuf = gdk_pixbuf_new_from_file (filename);

	if (image->filename)
		g_free (image->filename);

	if (image->pixbuf)
		image->filename = g_strdup (filename);
	else
		image->filename = NULL;

	return (image->pixbuf != NULL);
}

/**
 * image_load_pixbuf:
 * @image: An image structure.
 * @pixbuf: #GdkPixbuf with the rendered image.
 *
 * Loads an image from a #GdkPixbuf.
 *
 **/
void
image_load_pixbuf (Image *image, GdkPixbuf *pixbuf)
{
	g_return_if_fail (image != NULL);
	g_return_if_fail (pixbuf != NULL);

	if (image->pixbuf)
		gdk_pixbuf_unref (image->pixbuf);

	gdk_pixbuf_ref (pixbuf);
	image->pixbuf = pixbuf;

	if (image->filename)
		g_free (image->filename);
	image->filename = NULL;
}
