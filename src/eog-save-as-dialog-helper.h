#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "eog-uri-converter.h"


G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkWidget*    eog_save_as_dialog_new       (GtkWindow *main, GList *images, GFile *base_file);

G_GNUC_INTERNAL
EogURIConverter* eog_save_as_dialog_get_converter (GtkWidget *dlg);


G_END_DECLS
