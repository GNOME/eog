#ifndef __GPI_MGR_PIXBUF_H__
#define __GPI_MGR_PIXBUF_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gmacros.h>
#include <libgpi/gpi-mgr.h>

G_BEGIN_DECLS

typedef struct _GPIMgrPixbuf         GPIMgrPixbuf;
typedef struct _GPIMgrPixbufClass    GPIMgrPixbufClass;
typedef struct _GPIMgrPixbufPrivate  GPIMgrPixbufPrivate;
typedef struct _GPIMgrPixbufSettings GPIMgrPixbufSettings;

#define GPI_TYPE_MGR_PIXBUF     (gpi_mgr_pixbuf_get_type ())
#define GPI_MGR_PIXBUF(o)       (G_TYPE_CHECK_INSTANCE_CAST((o),GPI_TYPE_MGR_PIXBUF,GPIMgrPixbuf))
#define GPI_MGR_PIXBUF_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k),GPI_TYPE_MGR_PIXBUF,GPIMgrPixbufClass))
#define GPI_IS_MGR_PIXBUF(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o),GPI_TYPE_MGR_PIXBUF))

struct _GPIMgrPixbufSettings {
	gboolean show_overlap, show_cut, show_order;
	struct {gdouble x, y;} overlap;
	struct {gboolean x, y;} center;
	struct {gdouble x, y;} offset;
	gboolean fit_to_page;
	gdouble adjust_to;
	gboolean down_right;
};

struct _GPIMgrPixbuf {
	GPIMgr parent_instance;

	/* If you modify this, emit the corresponding signal. */
	GPIMgrPixbufSettings settings;

	GdkPixbuf *pixbuf;

	GPIMgrPixbufPrivate *priv;
};

struct _GPIMgrPixbufClass {
	GPIMgrClass parent_class;
};

GType          gpi_mgr_pixbuf_get_type (void);
GPIMgrPixbuf  *gpi_mgr_pixbuf_new      (GdkPixbuf *);

gboolean       gpi_mgr_pixbuf_get_dim  (GPIMgrPixbuf *,
			gdouble *rw, gdouble *rh,
			gdouble *aw, gdouble *ah, guint *pw, guint *ph,
			guint *nx, guint *ny, gdouble *lw, gdouble *lh);
void           gpi_mgr_pixbuf_get_dim2 (GPIMgrPixbuf *mgr,
			gdouble aw, gdouble ah, guint pw, guint ph,
			guint nx, guint ny, gdouble lw, gdouble lh,
			guint col, guint row,
			gboolean *fx, gboolean *fy, gboolean *lx, gboolean *ly,
			gdouble *iw, gdouble *ih, GdkPixbuf **p,
			gdouble *ix, gdouble *iy);

G_END_DECLS

#endif
