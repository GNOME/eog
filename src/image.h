#ifndef IMAGE_H
#define IMAGE_H

#include <gdk-pixbuf/gdk-pixbuf.h>



/* The main image structure */
typedef struct {
	/* Buffer with original image data */
	GdkPixBuf *buf;

	/* Color substitution tables */
	guchar *r_lut, *g_lut, *b_lut;
} Image;


#endif
