#ifndef _EOG_SAVE_DIALOG_H_
#define _EOG_SAVE_DIALOG_H_

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define EOG_TYPE_SAVE_DIALOG              (eog_save_dialog_get_type ())
#define EOG_SAVE_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_SAVE_DIALOG, EogSaveDialog))
#define EOG_SAVE_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_SAVE_DIALOG, EogSaveDialogClass))
#define EOG_IS_SAVE_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_SAVE_DIALOG))
#define EOG_IS_SAVE_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_SAVE_DIALOG))

typedef struct _EogSaveDialog EogSaveDialog;
typedef struct _EogSaveDialogClass EogSaveDialogClass;
typedef struct _EogSaveDialogPrivate EogSaveDialogPrivate;


struct _EogSaveDialog {
	GtkDialog dialog;

	EogSaveDialogPrivate *priv;
};

struct _EogSaveDialogClass {
	GtkDialogClass parent_class;
};


GType       eog_save_dialog_get_type         (void);
GtkWidget*  eog_save_dialog_new              (void);

void        eog_save_dialog_update           (EogSaveDialog *dlg, double fraction, const char *caption);

G_END_DECLS

#endif /* _EOG_SAVE_DIALOG_H_ */
