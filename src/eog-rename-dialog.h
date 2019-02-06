#ifndef _EOG_RENAME_DIALOG_HELPER_H_
#define _EOG_RENAME_DIALOG_HELPER_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkWidget       *eog_rename_dialog_new      (GtkWindow   *main,
                                             const gchar *prev_name);

G_GNUC_INTERNAL
const gchar *eog_rename_dialog_get_new_name (GtkWidget *dlg);


G_END_DECLS

#endif /* _EOG_SAVE_DIALOG_HELPER_H_ */
