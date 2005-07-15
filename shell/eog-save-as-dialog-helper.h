#ifndef _EOG_SAVE_AS_DIALOG_HELPER_H_
#define _EOG_SAVE_AS_DIALOG_HELPER_H_

#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include "eog-uri-converter.h"


G_BEGIN_DECLS


GtkWidget*    eog_save_as_dialog_new       (GtkWindow *main, GList *images, GnomeVFSURI *base_uri);

EogURIConverter* eog_save_as_dialog_get_converter (GtkWidget *dlg);


G_END_DECLS

#endif /* _EOG_SAVE_DIALOG_HELPER_H_ */
