#include "eog-pixbuf-util.h"

void
eog_pixbuf_flip_horizontal (GdkPixbuf *pixbuf)
{
	int x, y, i;
	int n_channels;
	int width, height;
	int rowstride;
	guchar *src;
	guchar *dest;
	guchar *buffer;
	guchar tmp;

	g_return_if_fail (pixbuf != NULL);

	g_object_ref (pixbuf);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	buffer = gdk_pixbuf_get_pixels (pixbuf);
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	for (y = 0; y < height; y++) {
		for (x = 0; x < (width / 2); x++) {
			src = buffer + y * rowstride + x * n_channels;
			dest = buffer + y * rowstride + (width - x - 1) * n_channels;

			for (i = 0; i < n_channels; i++) {
				tmp = dest[i];
				dest[i] = src[i];
				src[i] = tmp;
			}
		}
	}

	g_object_unref (pixbuf);
}

void
eog_pixbuf_flip_vertical (GdkPixbuf *pixbuf)
{
	int x, y, i;
	int n_channels;
	int width, height;
	int rowstride;
	guchar *src;
	guchar *dest;
	guchar *buffer;
	guchar tmp;

	g_return_if_fail (pixbuf != NULL);

	g_object_ref (pixbuf);
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	buffer = gdk_pixbuf_get_pixels (pixbuf);
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	for (x = 0; x < width; x++) {
		for (y = 0; y < (height / 2); y++) {
			src = buffer + y * rowstride + x * n_channels;
			dest = buffer + (height - y - 1) * rowstride +  x * n_channels;

			for (i = 0; i < n_channels; i++) {
				tmp = dest[i];
				dest[i] = src[i];
				src[i] = tmp;
			}
		}
	}
}

GdkPixbuf*
eog_pixbuf_rotate_90_cw (GdkPixbuf *pixbuf)
{
	guchar *src_buffer;
	guchar *dest_buffer;
	guchar *src_pos;
	guchar *dest_pos;
	int src_width;
	int src_height;
	int src_rowstride;
	int src_n_channels;
	int dest_width;
	int dest_height;
	int dest_rowstride;
	int dest_n_channels;
	int src_x, src_y, dest_x, dest_y, i;
	GdkPixbuf *dest;

	g_return_val_if_fail (pixbuf != NULL, NULL);

	g_object_ref (pixbuf);

	/* FIXME: Add in-place rotation when width == height. */

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);
	src_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	src_n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	src_buffer = gdk_pixbuf_get_pixels (pixbuf);

	dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
		                   gdk_pixbuf_get_has_alpha (pixbuf),
		                   gdk_pixbuf_get_bits_per_sample (pixbuf),
		                   src_height,
		                   src_width);

	if (dest == NULL) return NULL;

	dest_width = gdk_pixbuf_get_width (dest);
	dest_height = gdk_pixbuf_get_height (dest);
	dest_rowstride = gdk_pixbuf_get_rowstride (dest);
	dest_n_channels = gdk_pixbuf_get_n_channels (dest);
	dest_buffer = gdk_pixbuf_get_pixels (dest);

	for (src_y = 0; src_y < src_height; src_y++) {
		for (src_x = 0; src_x < src_width; src_x++) {
			src_pos = src_buffer + src_y * src_rowstride + src_x * src_n_channels;

			dest_x = dest_width - src_y - 1;
			dest_y = src_x;
			dest_pos = dest_buffer + dest_y * dest_rowstride + dest_x * dest_n_channels;

			for (i = 0; i < src_n_channels; i++) {
				dest_pos[i] = src_pos[i];
			}
		}
	}

	g_object_unref (pixbuf);

	return dest;
}

GdkPixbuf*
eog_pixbuf_rotate_90_ccw (GdkPixbuf *pixbuf)
{
	guchar *src_buffer;
	guchar *dest_buffer;
	guchar *src_pos;
	guchar *dest_pos;
	int src_width;
	int src_height;
	int src_rowstride;
	int src_n_channels;
	int dest_width;
	int dest_height;
	int dest_rowstride;
	int dest_n_channels;
	int src_x, src_y, dest_x, dest_y, i;
	GdkPixbuf *dest;

	g_return_val_if_fail (pixbuf != NULL, NULL);

	g_object_ref (pixbuf);

	/* FIXME: Add in-place rotation when width == height. */

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);
	src_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	src_n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	src_buffer = gdk_pixbuf_get_pixels (pixbuf);

	dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
		                   gdk_pixbuf_get_has_alpha (pixbuf),
		                   gdk_pixbuf_get_bits_per_sample (pixbuf),
		                   src_height,
		                   src_width);

	if (dest == NULL) return NULL;

	dest_width = gdk_pixbuf_get_width (dest);
	dest_height = gdk_pixbuf_get_height (dest);
	dest_rowstride = gdk_pixbuf_get_rowstride (dest);
	dest_n_channels = gdk_pixbuf_get_n_channels (dest);
	dest_buffer = gdk_pixbuf_get_pixels (dest);

	for (src_y = 0; src_y < src_height; src_y++) {
		for (src_x = 0; src_x < src_width; src_x++) {
			src_pos = src_buffer + src_y * src_rowstride + src_x * src_n_channels;

			dest_x = src_y;
			dest_y = dest_height - src_x - 1;
			dest_pos = dest_buffer + dest_y * dest_rowstride + dest_x * dest_n_channels;

			for (i = 0; i < src_n_channels; i++) {
				dest_pos[i] = src_pos[i];
			}
		}
	}

	g_object_unref (pixbuf);

	return dest;
}

void
eog_pixbuf_rotate_180 (GdkPixbuf *pixbuf)
{
	guchar *buffer;
	guchar *src_pos;
	guchar *dest_pos;
	guchar tmp;
	int width;
	int height;
	int rowstride;
	int n_channels;
	int src_x, src_y, dest_x, dest_y, i;

	g_return_if_fail (pixbuf != NULL);

	g_object_ref (pixbuf);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	buffer = gdk_pixbuf_get_pixels (pixbuf);

	for (src_y = 0; src_y < (height / 2); src_y++) {
		for (src_x = 0; src_x < width; src_x++) {
			src_pos = buffer + src_y * rowstride + src_x * n_channels;

			dest_x = width - src_x - 1;
			dest_y = height - src_y - 1;
			dest_pos = buffer + dest_y * rowstride + dest_x * n_channels;

			for (i = 0; i < n_channels; i++) {
				tmp = dest_pos[i];
				dest_pos[i] = src_pos[i];
				src_pos[i] = tmp;
			}
		}
	}

	if ((height % 2) == 1) {
		src_y = (height / 2);
		buffer = buffer + src_y * rowstride;
		for (src_x = 0; src_x < (width / 2); src_x++) {
			src_pos = buffer + src_x * n_channels;
			dest_x = width - src_x - 1;
			dest_y = src_y;
			dest_pos = buffer + dest_x * n_channels;

			for (i = 0; i < n_channels; i++) {
				tmp = dest_pos[i];
				dest_pos[i] = src_pos[i];
				src_pos[i] = tmp;
			}
		}
	}

	g_object_unref (pixbuf);
}

