/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-image-io.c - Image loading/saving for EogImage.
 *
 * Authors:
 *   Iain Holmes (ih@csd.abdn.ac.uk)
 *   Michael Zucchi (zucchi@zedzone.mmc.com.au)
 *   Federico Mena-Quintero (federico@gimp.org)
 *   Michael Fulbright (drmike@redhat.com)
 *   Martin Baulig (baulig@suse.de)
 *
 * Please refer to the individual image saving sections for information
 * about their authors and copyright.
 *
 * Copyright 1999-2000, Iain Holmes <ih@csd.abdn.ac.uk>
 * Copyright 1999, Michael Zucchi
 * Copyright 1999, The Free Software Foundation
 * Copyright 2000, SuSE GmbH.
 */

#include <config.h>
#include <gnome.h> /* Include this before png.h, or see lots of shadowed
		      variable warnings */
#include <stdio.h> /* Must be included, otherwise there is an error in png.h.
		      Bit wierd really. */
#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>

#include <bonobo.h>

#include <eog-image-io.h>

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
eog_image_save_xpm (EogImage *eog_image, Bonobo_Stream stream,
		    CORBA_Environment *ev)
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

	GdkPixbuf *pixbuf;

	g_return_val_if_fail (eog_image != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (eog_image), FALSE);
	g_return_val_if_fail (stream != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	pixbuf = eog_image_get_pixbuf (eog_image);
	if (!pixbuf)
		return FALSE;

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
	if (!retval)
	    bonobo_stream_client_write (stream, data, strlen (data)+1, ev);
	XpmFree (data);
  
	g_free (ibuff);
	g_hash_table_foreach (hash, free_hash_table, NULL);
	g_hash_table_destroy (hash);

	gdk_pixbuf_unref (pixbuf);

	return retval == 0;
}

#else /* don't HAVE_XPM */

gboolean
eog_image_save_xpm (EogImage *eog_image, Bonobo_Stream stream,
		    CORBA_Environment *ev)
{
	g_return_val_if_fail (eog_image != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (eog_image), FALSE);
	g_return_val_if_fail (stream != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

	return FALSE;
}

#endif /* don't HAVE_XPM */

/*** End of stuff from GIMP */

/* =======================================================================
 * Start PNG code.
 *
 * Based on:
 *   gnome-icon-edit/src/io.c
 *
 * Authors:
 *   Iain Holmes (ih@csd.abdn.ac.uk)
 *   David Welton (davidw@linuxcare.com)
 *   Michael Meeks (mmeeks@gnu.org)
 *
 * Copyright 1999-2000, Iain Holmes <ih@csd.abdn.ac.uk>
 *
 * =======================================================================
 */

#ifdef HAVE_PNG

#include <png.h>

typedef struct {
        Bonobo_Stream      stream;
        CORBA_Environment *ev;
} BStreamData;

static void
png_write_data_fn (png_structp png_ptr, png_bytep data, png_size_t len)
{
	BStreamData *sd = png_get_io_ptr (png_ptr);

	if (sd->ev->_major != CORBA_NO_EXCEPTION)
		return;

	bonobo_stream_client_write (sd->stream, data, len, sd->ev);
}

static void
png_flush_fn (png_structp png_ptr)
{
	g_warning ("Flush nothing");
}

gboolean
eog_image_save_png (EogImage *eog_image, Bonobo_Stream stream,
		    CORBA_Environment *ev)
{
	gint width, height, depth, rowstride;
	guchar *pixels;
	GdkPixbuf *pixbuf;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep row_ptr, data = NULL;
	png_color_8 sig_bit;
	png_text text[2];
	int x, y, j;
	int bpc, has_alpha;
	BStreamData sdata;

	g_return_val_if_fail (eog_image != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (eog_image), FALSE);
	g_return_val_if_fail (stream != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	pixbuf = eog_image_get_pixbuf (eog_image);
	if (!pixbuf)
		return FALSE;

	/* no image data? abort */
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	if (!pixels) {
		gdk_pixbuf_unref (pixbuf);
		return FALSE;
	}

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);
	if (!png_ptr) {
		gdk_pixbuf_unref (pixbuf);
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		gdk_pixbuf_unref (pixbuf);
		return FALSE;
	}
  
	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		gdk_pixbuf_unref (pixbuf);
		return FALSE;
	}

	sdata.stream = stream;
	sdata.ev     = ev;
	png_set_write_fn (png_ptr, &sdata, png_write_data_fn, png_flush_fn);

	bpc = gdk_pixbuf_get_bits_per_sample (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	depth = gdk_pixbuf_get_bits_per_sample (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	if (has_alpha) {
		png_set_IHDR (png_ptr, info_ptr, width, height, bpc,
			      PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
#ifdef WORDS_BIGENDIAN
		png_set_swap_alpha (png_ptr);
#else
		png_set_bgr (png_ptr);
#endif
	} else {
		png_set_IHDR (png_ptr, info_ptr, width, height, bpc,
			      PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			      PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		data = g_malloc (width * 3 * sizeof(char));
	}
	sig_bit.red = bpc;
	sig_bit.green = bpc;
	sig_bit.blue = bpc;
	sig_bit.alpha = bpc;

	/* Some text to go with the png image */
	text[0].key = "Title";
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = "GNOME Icon Editor";
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text    (png_ptr, info_ptr, text, 2);
	png_set_sBIT    (png_ptr, info_ptr, &sig_bit);
	png_write_info  (png_ptr, info_ptr);
	png_set_shift   (png_ptr, &sig_bit);
	png_set_packing (png_ptr);

	for (y = 0; y < height; y++) {
		if (has_alpha)
			row_ptr = (png_bytep)pixels;
		else {
			for (j = 0, x = 0; x < width; x++)
				memcpy (&(data [x * 3]), &(pixels [x * 3]), 3);

			row_ptr = (png_bytep)data;
		}
		png_write_rows (png_ptr, &row_ptr, 1);
		pixels += rowstride;
	}

	g_free (data);
	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, (png_infopp) NULL);

	gdk_pixbuf_unref (pixbuf);

	return TRUE;
}

#else /* don't HAVE_PNG */

gboolean
eog_image_save_png (EogImage *eog_image, Bonobo_Stream stream,
		    CORBA_Environment *ev)
{
	g_return_val_if_fail (eog_image != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (eog_image), FALSE);
	g_return_val_if_fail (stream != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

	return FALSE;
}

#endif /* don't HAVE_PNG */

/* =======================================================================
 * Start JPEG code.
 *
 * Based on:
 *   gtk+/gdk-pixbuf/io-png.c
 *
 * Authors:
 *   Michael Zucchi (zucchi@zedzone.mmc.com.au)
 *   Federico Mena-Quintero (federico@gimp.org)
 *   Michael Fulbright (drmike@redhat.com)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 1999, Michael Zucchi
 * Copyright 1999, The Free Software Foundation
 * Copyright 2000, SuSE GmbH
 *
 * =======================================================================
 */

#ifdef HAVE_JPEG

#include <jpeglib.h>

/* we are a "destination manager" as far as libjpeg is concerned */
#define JPEG_PROG_BUF_SIZE 4096

typedef struct {
	struct jpeg_destination_mgr pub;   /* public fields */

	JOCTET *buffer_start;

	Bonobo_Stream stream;
        CORBA_Environment *ev;
} my_dest_mgr;

typedef my_dest_mgr * my_dest_ptr;

static void
init_destination (j_compress_ptr cinfo)
{
	my_dest_mgr *dest = (my_dest_ptr) cinfo->dest;

	dest->buffer_start = g_malloc0 (JPEG_PROG_BUF_SIZE+1);

	dest->pub.next_output_byte = dest->buffer_start;
	dest->pub.free_in_buffer = JPEG_PROG_BUF_SIZE;
}

static boolean
empty_output_buffer (j_compress_ptr cinfo)
{
	my_dest_mgr *dest = (my_dest_ptr) cinfo->dest;

	if (dest->ev->_major != CORBA_NO_EXCEPTION) {
		jpeg_abort_compress (cinfo);
		return FALSE;
	}

	bonobo_stream_client_write (dest->stream,
				    dest->buffer_start, JPEG_PROG_BUF_SIZE,
				    dest->ev);

	if (dest->ev->_major != CORBA_NO_EXCEPTION) {
		jpeg_abort_compress (cinfo);
		return FALSE;
	}

	dest->pub.next_output_byte = dest->buffer_start;
	dest->pub.free_in_buffer = JPEG_PROG_BUF_SIZE;

	return TRUE;
}

static void
term_destination (j_compress_ptr cinfo)
{
	my_dest_mgr *dest = (my_dest_ptr) cinfo->dest;

	if (dest->ev->_major != CORBA_NO_EXCEPTION) {
		jpeg_abort_compress (cinfo);
		return;
	}

	bonobo_stream_client_write (dest->stream, dest->buffer_start,
				    JPEG_PROG_BUF_SIZE -
				    dest->pub.free_in_buffer,
				    dest->ev);

	if (dest->ev->_major != CORBA_NO_EXCEPTION) {
		jpeg_abort_compress (cinfo);
		return;
	}
}

/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	sigjmp_buf setjmp_buffer;
};

static void
fatal_error_handler (j_common_ptr cinfo)
{
	struct error_handler_data *errmgr;
        
	errmgr = (struct error_handler_data *) cinfo->err;
        
	siglongjmp (errmgr->setjmp_buffer, 1);

        g_assert_not_reached ();
}

static void
output_message_handler (j_common_ptr cinfo)
{
  /* This method keeps libjpeg from dumping crap to stderr */

  /* do nothing */
}

gboolean
eog_image_save_jpeg (EogImage *eog_image, Bonobo_Stream stream,
		     CORBA_Environment *ev)
{
	struct jpeg_compress_struct cinfo;
	guchar *buf = NULL;
	guchar *ptr;
	guchar *pixels = NULL;
	JSAMPROW *jbuf;
	int y = 0;
	int quality = 100; /* default; must be between 0 and 100 */
	int i, j;
	int w, h = 0;
	int rowstride = 0;
	struct error_handler_data jerr;
	GdkPixbuf *pixbuf;
	my_dest_mgr *dest;

	g_return_val_if_fail (eog_image != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (eog_image), FALSE);
	g_return_val_if_fail (stream != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	pixbuf = eog_image_get_pixbuf (eog_image);
	if (pixbuf == NULL)
		return FALSE;
     
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);

	/* no image data? abort */
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	if (pixels == NULL) {
		gdk_pixbuf_unref (pixbuf);
		return FALSE;
	}

	/* allocate a small buffer to convert image data */
	buf = g_malloc (w * 3 * sizeof (guchar));

	/* setup compress params */
	jpeg_create_compress (&cinfo);
	cinfo.image_width      = w;
	cinfo.image_height     = h;
	cinfo.input_components = 3; 
	cinfo.in_color_space   = JCS_RGB;

	/* set up our destination manager */
	cinfo.dest = (struct jpeg_destination_mgr *) g_new0 (my_dest_mgr, 1);
	dest = (my_dest_ptr) cinfo.dest;

	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;

	dest->stream = stream;
	dest->ev = ev;

	/* set up error handling */
	jerr.pub.error_exit = fatal_error_handler;
	jerr.pub.output_message = output_message_handler;
       
	cinfo.err = jpeg_std_error (&(jerr.pub));
	if (sigsetjmp (jerr.setjmp_buffer, 1)) {
		jpeg_destroy_compress (&cinfo);
		gdk_pixbuf_unref (pixbuf);
		g_free (dest->buffer_start);
		g_free (dest);
		free (buf);
		return FALSE;
	}

	/* set up jepg compression parameters */
	jpeg_set_defaults (&cinfo);
	jpeg_set_quality (&cinfo, quality, TRUE);
	jpeg_start_compress (&cinfo, TRUE);
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
	gdk_pixbuf_unref (pixbuf);
	free (buf);

	return TRUE;
}

#else /* don't HAVE_JPEG */

gboolean
eog_image_save_jpeg (EogImage *eog_image, Bonobo_Stream stream,
		     CORBA_Environment *ev)
{
	g_return_val_if_fail (eog_image != NULL, FALSE);
	g_return_val_if_fail (EOG_IS_IMAGE (eog_image), FALSE);
	g_return_val_if_fail (stream != CORBA_OBJECT_NIL, FALSE);
	g_return_val_if_fail (ev != NULL, FALSE);

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_Bonobo_Persist_WrongDataType, NULL);

	return FALSE;
}

#endif /* don't HAVE_JPEG */
