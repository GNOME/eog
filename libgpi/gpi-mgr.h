#ifndef __GPI_MGR_H__
#define __GPI_MGR_H__

#include <glib/gmacros.h>

G_BEGIN_DECLS

typedef struct _GPIMgr         GPIMgr;
typedef struct _GPIMgrClass    GPIMgrClass;
typedef struct _GPIMgrPrivate  GPIMgrPrivate;
typedef struct _GPIMgrSettings GPIMgrSettings;

#include <glib-object.h>
#include <libgnomeprint/gnome-print-job.h>

#define GPI_TYPE_MGR     (gpi_mgr_get_type ())
#define GPI_MGR(o)       (G_TYPE_CHECK_INSTANCE_CAST((o),GPI_TYPE_MGR,GPIMgr))
#define GPI_MGR_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k),GPI_TYPE_MGR,GPIMgrClass))
#define GPI_IS_MGR(o)    (G_TYPE_CHECK_INSTANCE_TYPE((o),GPI_TYPE_MGR))

struct _GPIMgrSettings {
	struct {gboolean print; gdouble top, bottom, right, left;} margin;
	struct {gdouble width, height;} page;
	enum {  GPI_MGR_SETTINGS_ORIENT_0,
		GPI_MGR_SETTINGS_ORIENT_90,
		GPI_MGR_SETTINGS_ORIENT_180,
		GPI_MGR_SETTINGS_ORIENT_270} orient;
};

struct _GPIMgr {
	GObject parent_instance;

	/* For now, read-write */
	GArray *selection;

	/* Read-only */
	GPIMgrSettings settings;

	GPIMgrPrivate *priv;
};

struct _GPIMgrClass {
	GObjectClass parent_class;

	void (* load_settings) (GPIMgr *);
	void (* save_settings) (GPIMgr *);

	guint          (* get_n_pages) (GPIMgr *);
	GnomePrintJob *(* get_job    ) (GPIMgr *);

	/* Signals */
	void (* settings_changed) (GPIMgr *);
};

GType    gpi_mgr_get_type (void);

void gpi_mgr_load_settings (GPIMgr *);
void gpi_mgr_save_settings (GPIMgr *);
void gpi_mgr_dump_settings (GPIMgr *);

/*
 * Preparing the actual printing.
 */
guint          gpi_mgr_get_n_pages (GPIMgr *);
GnomePrintJob *gpi_mgr_get_job     (GPIMgr *);

G_END_DECLS

#endif
