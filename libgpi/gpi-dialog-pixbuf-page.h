#ifndef _GPI_DIALOG_PIXBUF_PAGE_H_
#define _GPI_DIALOG_PIXBUF_PAGE_H_

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnomecanvas/gnome-canvas.h>

#include <libgpi/gpi-mgr-pixbuf.h>

G_BEGIN_DECLS

#define GPI_TYPE_DIALOG_PIXBUF_PAGE	   (gpi_dialog_pixbuf_page_get_type ())
#define GPI_DIALOG_PIXBUF_PAGE(obj)	   (GTK_CHECK_CAST ((obj), GPI_TYPE_DIALOG_PIXBUF_PAGE, GPIDialogPixbufPage))
#define GPI_DIALOG_PIXBUF_PAGE_CLASS(k)	   (GTK_CHECK_CLASS_CAST ((k), GPI_TYPE_DIALOG_PIXBUF_PAGE, GPIDialogPixbufPageClass))
#define GPI_IS_DIALOG_PIXBUF_PAGE(obj)     (GTK_CHECK_TYPE ((obj), GPI_TYPE_DIALOG_PIXBUF_PAGE))
#define GPI_IS_DIALOG_PIXBUF_PAGE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), GPI_TYPE_DIALOG_PIXBUF_PAGE))

typedef struct _GPIDialogPixbufPage		GPIDialogPixbufPage;
typedef struct _GPIDialogPixbufPagePrivate	GPIDialogPixbufPagePrivate;
typedef struct _GPIDialogPixbufPageClass	GPIDialogPixbufPageClass;

struct _GPIDialogPixbufPage
{
	GObject parent;

	GPIDialogPixbufPagePrivate	*priv;
};

struct _GPIDialogPixbufPageClass
{
	GObjectClass parent_class;
};

GtkType              gpi_dialog_pixbuf_page_get_type (void);
GPIDialogPixbufPage *gpi_dialog_pixbuf_page_new (GPIMgrPixbuf *,
						 GnomeCanvasGroup *, guint n);

void gpi_dialog_pixbuf_page_update_now (GPIDialogPixbufPage *);
void gpi_dialog_pixbuf_page_update     (GPIDialogPixbufPage *);

G_END_DECLS

#endif /* _GPI_DIALOG_PIXBUF_PAGE_H_ */

