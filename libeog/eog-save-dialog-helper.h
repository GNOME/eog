#ifndef _EOG_SAVE_DIALOG_HELPER_H_
#define _EOG_SAVE_DIALOG_HELPER_H_

#include <gtk/gtk.h>
#include "eog-image.h"
#include "eog-image-save-info.h"

G_BEGIN_DECLS

GtkWidget*    eog_save_dialog_new       (GtkWindow *main, int n_images);

void          eog_save_dialog_set_progress (GtkWindow *dlg, float progress);
void          eog_save_dialog_finished_image (GtkWindow *dlg);
void          eog_save_dialog_start_image (GtkWindow *dlg, EogImage *image, GnomeVFSURI *uri);

void          eog_save_dialog_close     (GtkWindow *dlg, gboolean successful);
GtkWidget*    eog_save_dialog_get_button (GtkWindow *dlg);
void          eog_save_dialog_cancel     (GtkWindow *dlg);



G_END_DECLS

#endif /* _EOG_SAVE_DIALOG_HELPER_H_ */
