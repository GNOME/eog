#ifndef _EOG_IMAGE_PRIVATE_H_
#define _EOG_IMAGE_PRIVATE_H_

#include "eog-image.h"

enum {
	EOG_IMAGE_LOAD_STATUS_NONE     = 0,
	EOG_IMAGE_LOAD_STATUS_PREPARED = 1 << 0,
	EOG_IMAGE_LOAD_STATUS_UPDATED  = 1 << 1,
	EOG_IMAGE_LOAD_STATUS_DONE     = 1 << 2,
	EOG_IMAGE_LOAD_STATUS_FAILED   = 1 << 3,
	EOG_IMAGE_LOAD_STATUS_CANCELLED = 1 << 4,
	EOG_IMAGE_LOAD_STATUS_INFO_DONE = 1 << 5,
	EOG_IMAGE_LOAD_STATUS_TRANSFORMED = 1 << 6,
	EOG_IMAGE_LOAD_STATUS_PROGRESS = 1 << 7
};

struct _EogImagePrivate {
	GnomeVFSURI *uri;
	EogImageLoadMode mode;

	GdkPixbuf *image;
	GdkPixbuf *thumbnail;
	
	gint width;
	gint height;
#if HAVE_EXIF
	ExifData *exif;
#endif

	gint thumbnail_id;
	
	gboolean modified;

	gchar *caption;
	gchar *caption_key;

	GThread *load_thread;
	gint load_id;
	GMutex *status_mutex;
	gint load_status;
	gboolean cancel_loading;
	float progress; /* Range from [0.0...1.0] indicate the progress of 
			   actions in percent */

	/* data which depends on the load status */
	int update_x1;
	int update_y1;
	int update_x2;
	int update_y2;
	char *error_message;
	
	/* stack of transformations recently applied */
	GList *undo_stack;
	/* composition of all applied transformations */
	EogTransform *trans;
};


#endif /* _EOG_IMAGE_PRIVATE_H_ */
