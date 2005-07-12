#ifndef _EOG_IMAGE_PRIVATE_H_
#define _EOG_IMAGE_PRIVATE_H_

#include <libgnomevfs/gnome-vfs-file-size.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif
#include "eog-image.h"
#include <lcms.h>

typedef enum {
	EOG_IMAGE_STATUS_UNKNOWN,
	EOG_IMAGE_STATUS_LOADING,
	EOG_IMAGE_STATUS_LOADED,
	EOG_IMAGE_STATUS_FAILED
} EogImageStatus;
 
struct _EogImagePrivate {
	GnomeVFSURI *uri;
	EogImageStatus status;

	GdkPixbuf *image;
	GdkPixbuf *thumbnail;
	
	gint width;
	gint height;
	GnomeVFSFileSize bytes;
	char *file_type;

	guchar  *exif_chunk; /* holds EXIF raw data */
	guint    exif_chunk_len;
	guchar  *iptc_chunk; /* holds IPTC raw data */
	guint    iptc_chunk_len;
#if HAVE_EXIF
	ExifData *exif;      /* this is mutual exclusive to exif_chunk. Only 
			      * either of these are not NULL:
			      */
#endif
	cmsHPROFILE profile;

	gint thumbnail_id;
	
	gboolean modified;

	gchar *caption;
	gchar *caption_key;

	GThread *load_thread;
	GMutex *status_mutex;
	GCond  *load_finished;
	gboolean cancel_loading;
	float progress; /* Range from [0.0...1.0] indicate the progress of 
			   actions in percent */

	char *error_message;
	
	/* stack of transformations recently applied */
	GList *undo_stack;
	/* composition of all applied transformations */
	EogTransform *trans;

	guint data_ref_count;
};


void eog_image_free_mem_private (EogImage *image);

#endif /* _EOG_IMAGE_PRIVATE_H_ */
