#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <gtk/gtkmain.h>
#include <libart_lgpl/art_affine.h>
#include "eog-transform.h"

#define PROFILE 1

struct _EogTransformPrivate {
	double affine[6];
};

static void
eog_transform_finalize (GObject *object)
{
	EogTransform *trans = EOG_TRANSFORM (object);
	
	g_free (trans->priv);
}

static void
eog_transform_init (EogTransform *trans)
{
	EogTransformPrivate *priv;

	priv = g_new0 (EogTransformPrivate, 1);

	trans->priv = priv;
}

static void 
eog_transform_class_init (EogTransformClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_transform_finalize;
}


G_DEFINE_TYPE (EogTransform, eog_transform, G_TYPE_OBJECT)


GdkPixbuf*    
eog_transform_apply   (EogTransform *trans, GdkPixbuf *pixbuf)
{
	ArtPoint dest_top_left;
	ArtPoint dest_bottom_right;
	ArtPoint vertices[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
	double r_det;
	int inverted [6];
	ArtPoint dest;
	ArtPoint src;
	
	int src_width;
	int src_height;
	int src_rowstride;
	int src_n_channels;
	guchar *src_buffer;

	GdkPixbuf *dest_pixbuf;
	int dest_width;
	int dest_height;
	int dest_rowstride;
	int dest_n_channels;
	guchar *dest_buffer;

	guchar *src_pos;
	guchar *dest_pos;
	int dx, dy, sx, sy;
	int i, x, y;

	g_return_val_if_fail (pixbuf != NULL, NULL);

	g_object_ref (pixbuf);

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);
	src_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	src_n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	src_buffer = gdk_pixbuf_get_pixels (pixbuf);

	/* find out the dimension of the destination pixbuf */
	dest_top_left.x = 100000;
	dest_top_left.y = 100000;
	dest_bottom_right.x = -100000;
	dest_bottom_right.y = -100000;

	for (i = 0; i < 4; i++) {
		src.x = vertices[i].x * (src_width - 1);
		src.y = vertices[i].y * (src_height -1);

		art_affine_point (&dest, &src, trans->priv->affine);

		dest_top_left.x = MIN (dest_top_left.x, dest.x);
		dest_top_left.y = MIN (dest_top_left.y, dest.y);

		dest_bottom_right.x = MAX (dest_bottom_right.x, dest.x);
		dest_bottom_right.y = MAX (dest_bottom_right.y, dest.y);
	}
	
	/* create the resulting pixbuf */
	dest_width = abs (dest_bottom_right.x - dest_top_left.x + 1);
	dest_height = abs (dest_bottom_right.y - dest_top_left.y + 1);

	dest_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			       gdk_pixbuf_get_has_alpha (pixbuf),
			       gdk_pixbuf_get_bits_per_sample (pixbuf),
			       dest_width,
			       dest_height);
	dest_rowstride = gdk_pixbuf_get_rowstride (dest_pixbuf);
	dest_n_channels = gdk_pixbuf_get_n_channels (dest_pixbuf);
	dest_buffer = gdk_pixbuf_get_pixels (dest_pixbuf);

	/* invert the matrix so that we can compute the source pixel
	   from the target pixel and convert the values to integer
	   ones (faster!)  FIXME: Maybe we can do some more
	   improvements by using special mmx/3dnow features if
	   available.
	*/
	r_det = 1.0 / (trans->priv->affine[0] * trans->priv->affine[3] - trans->priv->affine[1] * trans->priv->affine[2]);
	inverted[0] =  trans->priv->affine[3] * r_det;
	inverted[1] = -trans->priv->affine[1] * r_det;
	inverted[2] = -trans->priv->affine[2] * r_det;
	inverted[3] =  trans->priv->affine[0] * r_det;
	inverted[4] = -trans->priv->affine[4] * inverted[0] - trans->priv->affine[5] * inverted[2];
	inverted[5] = -trans->priv->affine[4] * inverted[1] - trans->priv->affine[5] * inverted[3];

	/* for every destination pixel (dx,dy) compute the source pixel (sx, sy) and copy the
	   color values */
	for (y = 0, dy = dest_top_left.y; y < dest_height; y++, dy++) {
		for (x = 0, dx = dest_top_left.x; x < dest_width; x++, dx++) {

			sx = dx * inverted[0] + dy * inverted[2] + inverted[4];
			sy = dx * inverted[1] + dy * inverted[3] + inverted[5];

			if (sx >= 0 && sx < src_width && sy >= 0 && sy < src_height) {
				src_pos  = src_buffer  + sy * src_rowstride  + sx * src_n_channels;
				dest_pos = dest_buffer +  y * dest_rowstride +  x * dest_n_channels;
			
				for (i = 0; i <  src_n_channels; i++) {
					dest_pos[i] = src_pos[i];
				}
			}
		}
	}

	g_object_unref (pixbuf);

	return dest_pixbuf;
}

EogTransform* 
eog_transform_reverse (EogTransform *trans)
{
	EogTransform *reverse; 

	g_return_val_if_fail (EOG_IS_TRANSFORM (trans), NULL);

	reverse = EOG_TRANSFORM (g_object_new (EOG_TYPE_TRANSFORM, 0));

	art_affine_invert (reverse->priv->affine, trans->priv->affine);

	return reverse;
}

EogTransform*
eog_transform_compose (EogTransform *trans, EogTransform *compose)
{
	EogTransform *composition;

	g_return_val_if_fail (EOG_IS_TRANSFORM (trans), NULL);
	g_return_val_if_fail (EOG_IS_TRANSFORM (compose), NULL);
	
	composition = EOG_TRANSFORM (g_object_new (EOG_TYPE_TRANSFORM, 0));

	art_affine_multiply (composition->priv->affine,
			     trans->priv->affine,
			     compose->priv->affine);

	return composition;
}

gboolean 
eog_transform_is_identity (EogTransform *trans)
{
	double identity[6] = { 1, 0, 0, 1, 0, 0 };

	g_return_val_if_fail (EOG_IS_TRANSFORM (trans), FALSE);

	return art_affine_equal (identity, trans->priv->affine);
}

EogTransform*
eog_transform_identity_new ()
{
	EogTransform *trans; 

	trans = EOG_TRANSFORM (g_object_new (EOG_TYPE_TRANSFORM, 0));
	
	art_affine_identity (trans->priv->affine);

	return trans;
}

EogTransform* 
eog_transform_rotate_new (int degree)
{
	EogTransform *trans; 

	trans = EOG_TRANSFORM (g_object_new (EOG_TYPE_TRANSFORM, 0));
	
	art_affine_rotate (trans->priv->affine, degree);

	return trans;
}

EogTransform* 
eog_transform_flip_new   (EogTransformType type)
{
	EogTransform *trans; 
	gboolean horiz, vert;

	trans = EOG_TRANSFORM (g_object_new (EOG_TYPE_TRANSFORM, 0));
	
	art_affine_identity (trans->priv->affine);

	horiz = (type == EOG_TRANSFORM_FLIP_HORIZONTAL);
	vert = (type == EOG_TRANSFORM_FLIP_VERTICAL);

	art_affine_flip (trans->priv->affine,
			 trans->priv->affine, 
			 horiz, vert);

	return trans;
}

EogTransform* 
eog_transform_scale_new  (double sx, double sy)
{
	EogTransform *trans; 

	trans = EOG_TRANSFORM (g_object_new (EOG_TYPE_TRANSFORM, 0));
	
	art_affine_scale (trans->priv->affine, sx, sy);

	return trans;
}

EogTransform*
eog_transform_new (EogTransformType type)
{
	EogTransform *trans = NULL;
	EogTransform *temp1 = NULL, *temp2 = NULL;

	switch (type) {
	case EOG_TRANSFORM_NONE:
		trans = eog_transform_identity_new ();
		break;
	case EOG_TRANSFORM_FLIP_HORIZONTAL:
		trans = eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL);
		break;
	case EOG_TRANSFORM_ROT_180:
		trans = eog_transform_rotate_new (180);
		break;
	case EOG_TRANSFORM_FLIP_VERTICAL:
		trans = eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL);
		break;
	case EOG_TRANSFORM_TRANSPOSE:
		temp1 = eog_transform_rotate_new (90);
		temp2 = eog_transform_flip_new (EOG_TRANSFORM_FLIP_HORIZONTAL);
		trans = eog_transform_compose (temp1, temp2);
		g_object_unref (temp1);
		g_object_unref (temp2);
		break;
	case EOG_TRANSFORM_ROT_90:
		trans = eog_transform_rotate_new (90);
		break;
	case EOG_TRANSFORM_TRANSVERSE:
		temp1 = eog_transform_rotate_new (90);
		temp2 = eog_transform_flip_new (EOG_TRANSFORM_FLIP_VERTICAL);
		trans = eog_transform_compose (temp1, temp2);
		g_object_unref (temp1);
		g_object_unref (temp2);
		break;
	case EOG_TRANSFORM_ROT_270:
		trans = eog_transform_rotate_new (270);
		break;
	default:
		trans = eog_transform_identity_new ();
		break;
	}

	return trans;
}

EogTransformType
eog_transform_get_transform_type (EogTransform *trans)
{
	double affine[6];
	EogTransformPrivate *priv;

	g_return_val_if_fail (EOG_IS_TRANSFORM (trans), EOG_TRANSFORM_NONE);

	priv = trans->priv;

        art_affine_rotate (affine, 90);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_ROT_90;
	}

        art_affine_rotate (affine, 180);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_ROT_180;
	}

        art_affine_rotate (affine, 270);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_ROT_270;
	}

	art_affine_identity (affine);
	art_affine_flip (affine, affine, TRUE, FALSE);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_FLIP_HORIZONTAL;
	}

	art_affine_identity (affine);
	art_affine_flip (affine, affine, FALSE, TRUE);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_FLIP_VERTICAL;
	}

	art_affine_rotate (affine, 90);
	art_affine_flip (affine, affine, TRUE, FALSE);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_TRANSPOSE;
	}

	art_affine_rotate (affine, 90);
	art_affine_flip (affine, affine, FALSE, TRUE);
	if (art_affine_equal (affine, priv->affine)) {
		return EOG_TRANSFORM_TRANSVERSE;
	}

	return EOG_TRANSFORM_NONE;
}
