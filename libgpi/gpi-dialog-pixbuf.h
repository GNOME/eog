#ifndef __GPI_DIALOG_PIXBUF_H__
#define __GPI_DIALOG_PIXBUF_H__

#include <glib/gmacros.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgpi/gpi-dialog.h>

G_BEGIN_DECLS

typedef struct _GPIDialogPixbufPrivate GPIDialogPixbufPrivate;
typedef struct _GPIDialogPixbufClass   GPIDialogPixbufClass;
typedef struct _GPIDialogPixbuf        GPIDialogPixbuf;

#define GPI_TYPE_DIALOG_PIXBUF     (gpi_dialog_pixbuf_get_type ())
#define GPI_DIALOG_PIXBUF(o)       (G_TYPE_CHECK_INSTANCE_CAST((o),GPI_TYPE_DIALOG_PIXBUF,GPIDialogPixbuf))
#define GPI_DIALOG_PIXBUF_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k),GPI_TYPE_DIALOG_PIXBUF,GPIDialogPixbufClass))
#define GPI_IS_DIALOG_PIXBUF(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o),GPI_TYPE_DIALOG_PIXBUF))

struct _GPIDialogPixbuf {
	GPIDialog parent_instance;
	GPIDialogPixbufPrivate *priv;
};

struct _GPIDialogPixbufClass {
	GPIDialogClass parent_class;
};

GType            gpi_dialog_pixbuf_get_type (void);
GPIDialogPixbuf *gpi_dialog_pixbuf_new      (GdkPixbuf *);

G_END_DECLS

#endif /* __GPI_DIALOG_PIXBUF_H__ */
