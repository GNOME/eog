#ifndef __GPI_DIALOG_H__
#define __GPI_DIALOG_H__

#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef struct _GPIDialogPrivate GPIDialogPrivate;
typedef struct _GPIDialogClass   GPIDialogClass;
typedef struct _GPIDialog        GPIDialog;

#include <libgpi/gpi-mgr.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtknotebook.h>

#include <libgnomecanvas/gnome-canvas.h>

#define GPI_TYPE_DIALOG     (gpi_dialog_get_type ())
#define GPI_DIALOG(o)       (G_TYPE_CHECK_INSTANCE_CAST((o),GPI_TYPE_DIALOG,GPIDialog))
#define GPI_DIALOG_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k),GPI_TYPE_DIALOG,GPIDialogClass))
#define GPI_IS_DIALOG(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o),GPI_TYPE_DIALOG))

struct _GPIDialog {
	GtkDialog parent_instance;

	/* UI */
	GtkNotebook *notebook;

	/* Backend */
	GPIMgr *mgr;

	GPIDialogPrivate *priv;
};

struct _GPIDialogClass {
	GtkDialogClass parent_class;

	void (* ok_clicked           ) (GPIDialog *);
	void (* cancel_clicked       ) (GPIDialog *);
};

GType gpi_dialog_get_type  (void);
void  gpi_dialog_construct (GPIDialog *, GPIMgr *);

G_END_DECLS

#endif /* __GPI_DIALOG_H__ */
