/* This code is based on the jpeg saving code from gdk-pixbuf. Full copyright 
 * notice is given in the following:
 */
/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Michael Zucchi
 * Copyright (C) 1999 The Free Software Foundation
 * 
 * Progressive loading code Copyright (C) 1999 Red Hat, Inc.
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Michael Fulbright <drmike@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "eog-image-jpeg.h"

#if HAVE_JPEG

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpegtran.h>
#include <jpeglib.h>
#include <jerror.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif
#include "eog-image-private.h"

static char*
get_tmp_filepath (void)
{
	char *tmp_file;
	char *tmp_file_name;
	static int file_count = 0;

	tmp_file_name = g_strdup_printf ("eog%i%i.tmp", getpid (), file_count++);
	tmp_file = g_build_filename (g_get_tmp_dir (), tmp_file_name, NULL);

	g_free (tmp_file_name);

	return tmp_file;
}

static GnomeVFSResult
move_file_to_uri (char *file_path, GnomeVFSURI *uri)
{
	GnomeVFSResult result;
	GnomeVFSURI *source_uri;

	source_uri = gnome_vfs_uri_new (file_path);
		
	result = gnome_vfs_xfer_uri (source_uri,
				     uri, 
				     GNOME_VFS_XFER_DELETE_ITEMS,           /* delete source file */
				     GNOME_VFS_XFER_ERROR_MODE_ABORT,       /* abort on all errors */
				     GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, /* we checked for existing 
									       file already */
				     NULL,                                  /* no progress callback */
				     NULL);

	gnome_vfs_uri_unref (source_uri);

	return result;
}	


gboolean
eog_image_jpeg_save (EogImage *image, GnomeVFSURI *uri, GError **error)
{
	EogImagePrivate *priv;
	GdkPixbuf *pixbuf;
	GnomeVFSResult vfs_result;
	char *tmp_path;
	struct jpeg_compress_struct cinfo;
	guchar *buf = NULL;
	guchar *ptr;
	guchar *pixels = NULL;
	JSAMPROW *jbuf;
	int y = 0;
	volatile int quality = 75; /* default; must be between 0 and 100 */
	int i, j;
	int w, h = 0;
	int rowstride = 0;
	FILE *outfile;
	struct jpeg_error_mgr jerr;
#if HAVE_EXIF
	unsigned char *exif_buf;
	unsigned int   exif_buf_len;
#endif
	
	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	
	priv = image->priv;

	pixbuf = priv->image;
	if (pixbuf == NULL) {
		return FALSE;
	}
	
	tmp_path = get_tmp_filepath ();
	outfile = fopen (tmp_path, "wb");
	if (outfile == NULL) {
		return FALSE;
	}

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	
	/* no image data? abort */
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	g_return_val_if_fail (pixels != NULL, FALSE);
	
	/* allocate a small buffer to convert image data */
	buf = g_try_malloc (w * 3 * sizeof (guchar));
	if (!buf) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			     _("Couldn't allocate memory for loading JPEG file"));
		return FALSE;
	}
	
	/* set up error handling */
	
	cinfo.err = jpeg_std_error (&jerr);
	
	/* setup compress params */
	jpeg_create_compress (&cinfo);
	jpeg_stdio_dest (&cinfo, outfile);
	cinfo.image_width      = w;
	cinfo.image_height     = h;
	cinfo.input_components = 3; 
	cinfo.in_color_space   = JCS_RGB;
	
	/* set up jepg compression parameters */
	jpeg_set_defaults (&cinfo);
	jpeg_set_quality (&cinfo, quality, TRUE);
	jpeg_start_compress (&cinfo, TRUE);
	
#if HAVE_EXIF
	if (priv->exif != NULL)
	{
		g_print ("save exif data\n");
		exif_data_save_data (priv->exif, &exif_buf, &exif_buf_len);
		jpeg_write_marker (&cinfo, 0xe1, exif_buf, exif_buf_len);
		g_free (exif_buf);
	}
#endif
	/* get the start pointer */
	ptr = pixels;
	/* go one scanline at a time... and save */
	i = 0;
	while (cinfo.next_scanline < cinfo.image_height) {
		/* convert scanline from ARGB to RGB packed */
		for (j = 0; j < w; j++)
			memcpy (&(buf[j*3]), &(ptr[i*rowstride + j*3]), 3);
		
		/* write scanline */
		jbuf = (JSAMPROW *)(&buf);
		jpeg_write_scanlines (&cinfo, jbuf, 1);
		i++;
		y++;
		
	}
	
	/* finish off */
	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress(&cinfo);
	g_free (buf);

	fclose (outfile);

	/* move temporary file to final destination */
	vfs_result = move_file_to_uri (tmp_path, uri);
	if (vfs_result != GNOME_VFS_OK) {
		g_set_error (error, EOG_IMAGE_ERROR,
			     EOG_IMAGE_ERROR_VFS, 
			     gnome_vfs_result_to_string (vfs_result));
	}
	g_free (tmp_path);

	return (vfs_result == GNOME_VFS_OK);
}

gboolean 
eog_image_jpeg_save_lossless (EogImage *image, GnomeVFSURI *uri, GError **error)
{
	const char *img_path;
	int result;
	JXFORM_CODE trans_code = JXFORM_NONE;
	EogTransformType transformation;
	char *tmp_file;
	GnomeVFSResult vfs_result = GNOME_VFS_OK;

	g_return_val_if_fail (EOG_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	img_path = gnome_vfs_uri_get_path (image->priv->uri);

	if (image->priv->trans != NULL) {
		transformation = eog_transform_get_transform_type (image->priv->trans);
		switch (transformation) {
		case EOG_TRANSFORM_ROT_90:
			trans_code = JXFORM_ROT_90; 
			break;
		case EOG_TRANSFORM_ROT_270:
			trans_code = JXFORM_ROT_270; 
			break;
		case EOG_TRANSFORM_ROT_180:
			trans_code = JXFORM_ROT_180; 
			break;
		case EOG_TRANSFORM_FLIP_HORIZONTAL:
			trans_code = JXFORM_FLIP_H;
			break;
		case EOG_TRANSFORM_FLIP_VERTICAL:
			trans_code = JXFORM_FLIP_V;
			break;
		default:
			trans_code = JXFORM_NONE;
			break;
		}
	}

	/* We save the resulting jpeg file to a temporary location
	 * first and move it to the final destination then. This way
	 * can also overwrite a file.
	 */
	tmp_file = get_tmp_filepath ();
	g_print ("tmp_file: %s\n", tmp_file);

	result = jpegtran ((char*) img_path, (char*) tmp_file, trans_code, error);

	if (result == 0) {
		/* image successfully written */
		vfs_result = move_file_to_uri (tmp_file, uri);
		if (vfs_result != GNOME_VFS_OK) {
			g_set_error (error, EOG_IMAGE_ERROR,
				     EOG_IMAGE_ERROR_VFS, 
				     gnome_vfs_result_to_string (vfs_result));
		}
	}

	if (g_file_test (tmp_file, G_FILE_TEST_EXISTS)) {
		gnome_vfs_unlink (tmp_file);
	}

	g_free (tmp_file);

	return ((result == 0) && (vfs_result == GNOME_VFS_OK));
}

#endif
