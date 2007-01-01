#ifndef _EOG_THUMBNAIL_H_
#define _EOG_THUMBNAIL_H_

#include "eog-jobs.h"

#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

void          eog_thumbnail_init (void);

GdkPixbuf*    eog_thumbnail_load (GnomeVFSURI *uri, GError **error);


G_END_DECLS

#endif /* _EOG_THUMBNAIL_H_ */
