#ifndef _EOG_IMAGE_PRIVATE_H_
#define _EOG_IMAGE_PRIVATE_H_

#include <libgnomevfs/gnome-vfs-file-size.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif
#include "eog-image.h"


typedef enum {
	EOG_IMAGE_STATUS_UNKNOWN,
	EOG_IMAGE_STATUS_LOADING,
	EOG_IMAGE_STATUS_LOADED,
	EOG_IMAGE_STATUS_FAILED
} EogImageStatus;
 
struct _EogImagePrivate {
	GnomeVFSURI *uri;
	EogImageLoadMode mode;
	EogImageStatus status;

	GdkPixbuf *image;
	GdkPixbuf *thumbnail;
	
	gint width;
	gint height;
	GnomeVFSFileSize bytes;
	char *file_type;
#if HAVE_EXIF
	ExifData *exif;
#endif

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
};


void eog_image_free_mem_private (EogImage *image);

#endif /* _EOG_IMAGE_PRIVATE_H_ */
