

#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif
#include <libgnome/gnome-i18n.h>

#include "eog-image-helper.h"


/* =======================================================================
 * Start XPM code.
 *
 * Based on:
 *   gnome-icon-edit/src/io.c
 *
 * Authors:
 *   Iain Holmes (ih@csd.abdn.ac.uk)
 *
 * Copyright 1999-2000, Iain Holmes <ih@csd.abdn.ac.uk>
 *
 * =======================================================================
 */

/* Based on code from the GIMP */

#ifdef HAVE_XPM

int cpp;
static const char linenoise [] =
" .+@#$%&*=-;>,')!~{]^/(_:<[}|1234567890abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ`";

typedef struct
{
	guchar r;
	guchar g;
	guchar b;
} rgbkey;

static guint
rgbhash (rgbkey *c)
{
	return ((guint)c->r) ^ ((guint)c->g) ^ ((guint)c->b);
}

static guint
compare (rgbkey *c1,
	 rgbkey *c2)
{
	return (c1->r == c2->r)&&(c1->g ==c2->g)&&(c1->b == c2->b);
}

static void
set_XpmImage (XpmColor *array,
	      guint ie_index,
	      gchar *colorstring)
{
	gchar *p;
	int i, charnum, indtemp;

	indtemp = ie_index;
	array[ie_index].string = p = g_new (gchar, cpp+1);

	/*convert the index number to base sizeof(linenoise)-1 */
	for(i=0; i<cpp; ++i) {
		charnum = indtemp%(sizeof(linenoise)-1);
		indtemp = indtemp / (sizeof (linenoise)-1);
		*p++=linenoise[charnum];
	}

	*p = '\0'; /* C and its stupid null-terminated strings...*/

	array[ie_index].symbolic = NULL;
	array[ie_index].m_color = NULL;
	array[ie_index].g4_color = NULL;
	array[ie_index].c_color = NULL;
	array[ie_index].g_color = colorstring;
}

static void
create_colormap_from_hash (gpointer gkey,
			   gpointer value,
			   gpointer user_data)
{
	rgbkey *key = gkey;
	char *string = g_new (char, 8);

	sprintf (string, "#%02X%02X%02X", (int)key->r, (int)key->g, (int)key->b);
	set_XpmImage(user_data, *((int *) value), string);
}

static void
free_hash_table (gpointer key,
		 gpointer value,
		 gpointer user_data)
{
	/* Free the rgbkey */
	g_free (key);
	/* Free the gint indexno */
	g_free (value);
}

gboolean
eog_image_helper_save_xpm (GdkPixbuf *pixbuf, const char* path, GError **error)
{
	gint width, height, has_alpha, rowstride;
	gint ncolors = 1;
	gint *indexno;
	XpmColor *colormap;
	XpmImage *image;
	guint *ibuff;
	guchar *pixels, *row;

	GHashTable *hash = NULL;
	int retval, x, y;
	char *data;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	ibuff = g_new (guint, width*height);
	hash = g_hash_table_new ((GHashFunc)rgbhash, (GCompareFunc) compare);

	for (y = 0; y < height; y++) {
		guint *idata = ibuff + (y * width);
		row = pixels + (y * rowstride);

		for (x = 0; x < width; x++) {
			rgbkey *key = g_new (rgbkey, 1);
			guchar a;

			key->r = *(row++);
			key->g = *(row++);
			key->b = *(row++);
			a = has_alpha ? *(row++) : 255;

			if (a < 127)
				*(idata++) = 0;
			else {
				indexno = g_hash_table_lookup (hash, key);
				if (!indexno) {
					indexno = g_new (int, 1);
					*indexno = ncolors++;
					g_hash_table_insert (hash, key, indexno);
					key = g_new (rgbkey, 1);
				}
				*(idata++) = *indexno;
			}
		}
	}

	colormap = g_new (XpmColor, ncolors);
	cpp = (double)1.0 + (double)log (ncolors) / (double)log (sizeof (linenoise) - 1.0);

	set_XpmImage (colormap, 0, "None");

	g_hash_table_foreach (hash, create_colormap_from_hash, colormap);

	image = g_new (XpmImage, 1);

	image->width = width;
	image->height = height;
	image->ncolors = ncolors;
	image->cpp = cpp;
	image->colorTable = colormap;
	image->data = ibuff;

	retval = XpmCreateBufferFromXpmImage (&data, image, NULL);
	if (retval == 0) {
		GnomeVFSResult result;
		GnomeVFSHandle *handle;
		GnomeVFSFileSize bytes_written = 0;

		result = gnome_vfs_open (&handle, path, GNOME_VFS_OPEN_WRITE);
		if (result == GNOME_VFS_OK) {
			result = gnome_vfs_write (handle, data, strlen (data) + 1, &bytes_written);
			gnome_vfs_close (handle);
		}

		if (result != GNOME_VFS_OK) {
			/* error on openening file for write */
			retval = 1;
			g_set_error (error, EOG_IMAGE_ERROR, 
				     EOG_IMAGE_ERROR_VFS,
				     gnome_vfs_result_to_string (result));
		}
	}
	XpmFree (data);

	g_free (ibuff);
	g_hash_table_foreach (hash, free_hash_table, NULL);
	g_hash_table_destroy (hash);

	return retval == 0;
}

#else /* don't HAVE_XPM */

gboolean
eog_image_helper_save_xpm (GdkPixbuf *pixbuf, const char* path, GError **error)
{
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	g_set_error (error, GDK_PIXBUF_ERROR,
		     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
		     _("Unsupported image type for saving: xpm"));

	return FALSE;
}

#endif /* don't HAVE_XPM */
