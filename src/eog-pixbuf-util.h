#ifndef _EOG_PIXBUF_UTIL_H_
#define _EOG_PIXBUF_UTIL_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs-uri.h>

GSList*          eog_pixbuf_get_savable_formats (void);

GdkPixbufFormat* eog_pixbuf_get_format_by_suffix (const char *suffix);

GdkPixbufFormat* eog_pixbuf_get_format_by_uri (const char *txt_uri);
GdkPixbufFormat* eog_pixbuf_get_format_by_vfs_uri (GnomeVFSURI *uri);

char*            eog_pixbuf_get_common_suffix (GdkPixbufFormat *format);

#endif /* _EOG_PIXBUF_UTIL_H_ */

