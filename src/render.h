#ifndef RENDER_H
#define RENDER_H

#include "image.h"



void render_image (Image *image, guchar *dest,
		   guint dest_width, guint dest_height, guint dest_rowstride,
		   double zoom, guint xofs, guint yofs,
		   guchar *r_lut, guchar *g_lut, guchar *b_lut,
		   guchar dark_check, guchar light_check);



#endif
