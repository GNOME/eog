#include <config.h>
#include "gpi-mgr-pixbuf.h"

#include <glib/gmem.h>

#include <string.h>

#include <gconf/gconf-client.h>

#define _(s) (s)

struct _GPIMgrPixbufPrivate {
	GPtrArray *pages;
};

#define ORDER_SIZE 20
#define ORDER_OFFSET 5

static GPIMgrClass *parent_class = NULL;

static void
gpi_mgr_pixbuf_finalize (GObject *object)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (object);

	g_object_unref (mgr->pixbuf);
	g_free (mgr->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gpi_mgr_pixbuf_load_settings (GPIMgr *m)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (m);
	GConfClient *c;

	c = gconf_client_get_default ();
	mgr->settings.show_overlap = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/show_overlap", NULL);
	mgr->settings.show_cut = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/show_cut", NULL);
	mgr->settings.show_order = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/show_order", NULL);
	mgr->settings.fit_to_page = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/fit_to_page", NULL);
	mgr->settings.adjust_to = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/adjust_to", NULL);
	mgr->settings.overlap.x = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/overlap_x", NULL);
	mgr->settings.overlap.y = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/overlap_y", NULL);
	mgr->settings.center.x = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/center_x", NULL);
	mgr->settings.center.y = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/center_y", NULL);
	mgr->settings.down_right = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/down_right", NULL);
	g_object_unref (c);

	/* First time */
	if (!mgr->settings.adjust_to) {
		mgr->settings.adjust_to = 100.;
		mgr->settings.fit_to_page = TRUE;
		mgr->settings.center.x = TRUE;
		mgr->settings.center.y = TRUE;
	}

	g_signal_emit_by_name (mgr, "settings_changed", 0);
}

static void
gpi_mgr_pixbuf_save_settings (GPIMgr *m)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (m);
	GConfClient *c;

	c = gconf_client_get_default ();
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/show_overlap",
		mgr->settings.show_overlap, NULL);
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/show_cut",
		mgr->settings.show_cut, NULL);
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/show_order",
		mgr->settings.show_order, NULL);
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/fit_to_page",
		mgr->settings.fit_to_page, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/adjust_to",
		mgr->settings.adjust_to, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/overlap_x",
		mgr->settings.overlap.x, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/overlap_y",
		mgr->settings.overlap.y, NULL);
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/center_x",
		mgr->settings.center.x, NULL);
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/center_y",
		mgr->settings.center.y, NULL); 
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/down_right",
		mgr->settings.down_right, NULL);
	g_object_unref (c);
}

#define PL(ctx,x1,y1,x2,y2)			\
{						\
	gnome_print_moveto ((ctx), (x1), (y1));	\
	gnome_print_lineto ((ctx), (x2), (y2));	\
	gnome_print_stroke ((ctx));		\
}
#define GPL gnome_print_lineto

static void
gpi_mgr_pixbuf_print_page (GPIMgrPixbuf *mgr, GnomePrintContext *ctx,
			   guint *current, guint col, guint row)
{
	double matrix [] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	gdouble aw, ah, lw, lh, iw, ih, ix, iy, rw, rh;
	guint nx, ny, pw, ph;
	gboolean fx, fy, lx, ly;
	GdkPixbuf *p = NULL;
	gdouble x1, y1, x2, y2;
	gchar *s;
	gboolean skip = FALSE;
	guint i;

	g_return_if_fail (GPI_IS_MGR_PIXBUF (mgr));
	g_return_if_fail (GNOME_IS_PRINT_CONTEXT (ctx));

	/*
	 * Make sure we do not print 
	 *  - more pages than requested and
	 *  - more pages than available
	 */
	(*current)++;
	g_return_if_fail (gpi_mgr_pixbuf_get_dim (mgr, &rw, &rh,
				&aw, &ah, &pw, &ph, &nx, &ny, &lw, &lh));
	if (*current > nx * ny) return;

	/* Do we skip this page? */
	if (GPI_MGR (mgr)->selection && GPI_MGR (mgr)->selection->len) {
		for (i = 0; i < GPI_MGR (mgr)->selection->len; i++)
			if (nx * row + col == g_array_index (
				GPI_MGR (mgr)->selection, guint, i)) break;
		if (i == GPI_MGR (mgr)->selection->len) skip = TRUE;
	}

	if (!skip) {
		s = g_strdup_printf ("%i", *current);
		gnome_print_beginpage (ctx, s); g_free (s);
		gnome_print_gsave (ctx);

		/*
		 * We are now in the bottom left corner. Move the starting
		 * point to the upper left corner and make gnome-print
		 * count downwards.
		 */
		gnome_print_translate (ctx, 0., rh);
		matrix[0] = 1.; matrix[1] = 0.; matrix[2] = 0.;
		matrix[3] = -1.; matrix[4] = 0.; matrix[5] = 0.;
		gnome_print_concat (ctx, matrix);
		switch (GPI_MGR (mgr)->settings.orient) {
		case GPI_MGR_SETTINGS_ORIENT_0:
			break;
		case GPI_MGR_SETTINGS_ORIENT_90:
			break;
		case GPI_MGR_SETTINGS_ORIENT_180:
			break;
		case GPI_MGR_SETTINGS_ORIENT_270:
			break;
		}
	}

	p = gdk_pixbuf_scale_simple (mgr->pixbuf, pw, ph, GDK_INTERP_HYPER);
	gpi_mgr_pixbuf_get_dim2 (mgr, aw, ah, pw, ph, nx, ny, lw, lh,
				 col, row, &fx, &fy, &lx, &ly,
				 &iw, &ih, &p, &ix, &iy);

	if (!skip) {

		/* Print the image */
		memset (&matrix, 0, sizeof (matrix));
		matrix[0] = iw; matrix[3] = -ih;
		matrix[4] = ix; matrix[5] = iy + ih;
		gnome_print_gsave (ctx); gnome_print_concat (ctx, matrix);
		if (gdk_pixbuf_get_has_alpha (p))
			gnome_print_rgbaimage (ctx, gdk_pixbuf_get_pixels (p),
				gdk_pixbuf_get_width (p),
				gdk_pixbuf_get_height (p),
				gdk_pixbuf_get_rowstride (p));
		else
			gnome_print_rgbimage (ctx, gdk_pixbuf_get_pixels (p),
				gdk_pixbuf_get_width (p),
				gdk_pixbuf_get_height (p),
				gdk_pixbuf_get_rowstride (p));
		gnome_print_grestore (ctx);

		/* Cutting help */
		if (mgr->settings.show_cut) {
			x1 = ix + iw; y1 = iy + ih;
			x2 = x1 + (rw - ix - iw) / 3.;
			y2 = rh - 2 * (rh - iy - ih) / 2.;
			PL (ctx, ix, 0.0, ix, 2 * iy / 3);
			PL (ctx, x1, 0.0, x1, 2 * iy / 3);
			PL (ctx, 0.0, iy, 2 * ix / 3, iy);
			PL (ctx, 0.0, y1, 2 * ix / 3, y1);
			PL (ctx, x2, y1, rw, y1);
			PL (ctx,  x2, iy, rw, iy);
			PL (ctx, ix, rh , ix, y2);
			PL (ctx, x1, rh, x1, y2);
		}

		/* Margins */
		if (GPI_MGR (mgr)->settings.margin.print) {
			x1 = GPI_MGR (mgr)->settings.margin.left;
			x2 = rw - GPI_MGR (mgr)->settings.margin.right;
			y1 = GPI_MGR (mgr)->settings.margin.top;
			y2 = rh - GPI_MGR (mgr)->settings.margin.bottom;
			gnome_print_rect_stroked (ctx, x1, y1, x2-x1, y2-y1);
		}

		/* Ordering help */
		if (mgr->settings.show_order && !fx) {
			x1 = GPI_MGR (mgr)->settings.margin.right -
						ORDER_SIZE - ORDER_OFFSET;
			y1 = (rh - ORDER_SIZE) / 2.;
			gnome_print_newpath (ctx);
			gnome_print_moveto (ctx, x1, y1 + ORDER_SIZE / 2.);
			GPL (ctx, x1 + ORDER_SIZE / 2., y1);
			GPL (ctx, x1 + ORDER_SIZE, y1);
			GPL (ctx, x1 + ORDER_SIZE, y1 + ORDER_SIZE);
			GPL (ctx, x1 + ORDER_SIZE / 2., y1 + ORDER_SIZE);
			gnome_print_closepath (ctx);
			gnome_print_stroke (ctx);
		}
		if (mgr->settings.show_order && !fy) {
			x1 = (rw - ORDER_SIZE) / 2.;
			y1 = GPI_MGR (mgr)->settings.margin.top - 
				ORDER_SIZE - ORDER_OFFSET;
			gnome_print_newpath (ctx);
			gnome_print_moveto (ctx, x1, y1 + ORDER_SIZE / 2.);
			GPL (ctx, x1 + ORDER_SIZE / 2., y1);
			GPL (ctx, x1 + ORDER_SIZE, y1 + ORDER_SIZE / 2.);
			GPL (ctx, x1 + ORDER_SIZE, y1 + ORDER_SIZE);
			GPL (ctx, x1, y1 + ORDER_SIZE);
			gnome_print_closepath (ctx);
			gnome_print_stroke (ctx);
		}
		if (mgr->settings.show_order && !lx) {
			x1 = rw + ORDER_OFFSET -
				GPI_MGR (mgr)->settings.margin.right;
			y1 = (rh - ORDER_SIZE) / 2.;
			gnome_print_newpath (ctx);
			gnome_print_moveto (ctx, x1, y1);
			GPL (ctx, x1 + ORDER_SIZE / 2., y1);
			GPL (ctx, x1 + ORDER_SIZE, y1 + ORDER_SIZE / 2.);
			GPL (ctx, x1 + ORDER_SIZE / 2., y1 + ORDER_SIZE);
			GPL (ctx, x1, y1 + ORDER_SIZE);
			gnome_print_closepath (ctx);
			gnome_print_stroke (ctx);
		}
		if (mgr->settings.show_order && !ly) {
			x1 = (rw - ORDER_SIZE) / 2.;
			y1 = rh + ORDER_OFFSET -
				GPI_MGR (mgr)->settings.margin.bottom;
			gnome_print_newpath (ctx);
			gnome_print_moveto (ctx, x1, y1 + ORDER_SIZE / 2.);
			GPL (ctx, x1, y1);
			GPL (ctx, x1 + ORDER_SIZE, y1);
			GPL (ctx, x1 + ORDER_SIZE, y1 + ORDER_SIZE / 2.);
			GPL (ctx, x1 + ORDER_SIZE / 2., y1 + ORDER_SIZE);
			gnome_print_closepath (ctx);
			gnome_print_stroke (ctx);
		}

		/* Overlap help */
		if (mgr->settings.show_overlap) {
			x1 = ix + iw - mgr->settings.overlap.x;
			y1 = iy + ih - mgr->settings.overlap.y;
			y2 = rh / 100.; gnome_print_setdash (ctx, 1, &y2, 0);
			if (!lx) PL (ctx, x1, 0., x1, rh);
			y2 = rw / 100.; gnome_print_setdash (ctx, 1, &y2, 0);
			if (!ly) PL (ctx, 0., y1, rw, y1);
			gnome_print_setdash (ctx, 0, NULL, 0);
		}

		gnome_print_grestore (ctx);
		gnome_print_showpage (ctx);
	}
	g_object_unref (p);

	/* Print other pages */
	if (mgr->settings.down_right) {
		if (ly) /* Go right */
			gpi_mgr_pixbuf_print_page (mgr, ctx,
				current, col + 1, 0);
		else
			gpi_mgr_pixbuf_print_page (mgr, ctx,
				current, col, row + 1);
	} else {
		if (lx) /* Go down */
			gpi_mgr_pixbuf_print_page (mgr, ctx,
				current, 0, row + 1);
		else
			gpi_mgr_pixbuf_print_page (mgr, ctx,
				current, col + 1, row);
	}
}

static GnomePrintJob *
gpi_mgr_pixbuf_get_job (GPIMgr *m)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (m);
	GnomePrintJob *job;
	GnomePrintConfig *c;
	guint current = 0;
	const GnomePrintUnit *u;
	gdouble w, h;

	g_assert ((job = gnome_print_job_new (NULL)));

	u = gnome_print_unit_get_identity (GNOME_PRINT_UNIT_DIMENSIONLESS);

	/* Prepare the printing */
	g_assert ((c = gnome_print_job_get_config (job)));
	gnome_print_config_set_length (c, GNOME_PRINT_KEY_PAGE_MARGIN_RIGHT,
				       m->settings.margin.right, u);
	gnome_print_config_set_length (c, GNOME_PRINT_KEY_PAGE_MARGIN_LEFT,
				       m->settings.margin.left, u);
	gnome_print_config_set_length (c, GNOME_PRINT_KEY_PAGE_MARGIN_BOTTOM,
				       m->settings.margin.bottom, u);
	gnome_print_config_set_length (c, GNOME_PRINT_KEY_PAGE_MARGIN_TOP,
				       m->settings.margin.top, u);
	switch (m->settings.orient) {
	case GPI_MGR_SETTINGS_ORIENT_90:
	case GPI_MGR_SETTINGS_ORIENT_270:
		w = m->settings.page.height; h = m->settings.page.width;
		break;
	default:
		w = m->settings.page.width; h = m->settings.page.height;
		break;
	}
	gnome_print_config_set_length (c, GNOME_PRINT_KEY_PAPER_WIDTH, w, u);
	gnome_print_config_set_length (c, GNOME_PRINT_KEY_PAPER_HEIGHT, h, u);

	/* Print */
	gpi_mgr_pixbuf_print_page (mgr, gnome_print_job_get_context (job),
				   &current, 0, 0);
	gnome_print_job_close (job);

	return job;
}

static void
gpi_mgr_pixbuf_class_init (gpointer klass, gpointer class_data)
{
	GObjectClass *gobject_class;
	GPIMgrClass *mgr_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = gpi_mgr_pixbuf_finalize;

	mgr_class = GPI_MGR_CLASS (klass);
	mgr_class->load_settings = gpi_mgr_pixbuf_load_settings;
	mgr_class->save_settings = gpi_mgr_pixbuf_save_settings;
	mgr_class->get_job       = gpi_mgr_pixbuf_get_job;

	parent_class = g_type_class_peek_parent (klass);
}

static void
gpi_mgr_pixbuf_instance_init (GTypeInstance *instance, gpointer klass)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (instance);

	mgr->priv = g_new0 (GPIMgrPixbufPrivate, 1);
}

GType
gpi_mgr_pixbuf_get_type (void)
{
	static GType t = 0;

	if (!t) {
		static const GTypeInfo i = {
			sizeof (GPIMgrPixbufClass), NULL, NULL,
			gpi_mgr_pixbuf_class_init, NULL, NULL,
			sizeof (GPIMgrPixbuf), 0,
			gpi_mgr_pixbuf_instance_init, NULL
		};

		t = g_type_register_static (GPI_TYPE_MGR, "GPIMgrPixbuf",
					    &i, 0);
	}

	return t;
}

GPIMgrPixbuf *
gpi_mgr_pixbuf_new (GdkPixbuf *pixbuf)
{
	GPIMgrPixbuf *mgr;

	mgr = g_object_new (GPI_TYPE_MGR_PIXBUF, NULL);
	g_object_ref (mgr->pixbuf = pixbuf);

	gpi_mgr_load_settings (GPI_MGR (mgr));

	return mgr;
}

void
gpi_mgr_pixbuf_get_dim2 (GPIMgrPixbuf *mgr, gdouble aw, gdouble ah,
	guint pw, guint ph, guint nx, guint ny, gdouble lw, gdouble lh,
	guint col, guint row,
	gboolean *fx, gboolean *fy, gboolean *lx, gboolean *ly,
	gdouble *iw, gdouble *ih, GdkPixbuf **p, gdouble *ix, gdouble *iy)
{
	GdkPixbuf *pixbuf;
	gdouble x, y;

	g_return_if_fail (GPI_IS_MGR_PIXBUF (mgr));
	g_return_if_fail (GDK_IS_PIXBUF (*p));

	*fx = (col == 0); *lx = (nx == col + 1);
	*fy = (row == 0); *ly = (ny == row + 1);

	/* Width of the target image */
	if (*fx && *lx) *iw = pw;
	else if (*lx) {
		*iw = pw - (nx - 1) * (aw - mgr->settings.overlap.x);
		*iw += (mgr->settings.center.x ? lw/2.:mgr->settings.offset.x);
	} else {
		*iw = aw;
		if (*fx) *iw -= (mgr->settings.center.x ? lw / 2. :
						mgr->settings.offset.x);
	}
	if ((gint) *iw < 1) (*iw)++;

	/* Height of the target image */
	if (*fy && *ly) *ih = ph;
	else if (*ly) {
		*ih = ph - (ny - 1) * (ah - mgr->settings.overlap.y);
		*ih += (mgr->settings.center.y ? lh/2.:mgr->settings.offset.y);
	} else {
		*ih = ah;
		if (*fy) *ih -= (mgr->settings.center.y ? lh / 2. :
						mgr->settings.offset.y);
	}
	if ((gint) *ih < 1) (*ih)++;

	pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (*p),
			     gdk_pixbuf_get_has_alpha (*p),
			     gdk_pixbuf_get_bits_per_sample (*p),
			     (gint) *iw, (gint) *ih);
	if (*fx) x = 0.; else if (*lx) x = pw - *iw;
	else {
		x = (aw - mgr->settings.overlap.x) * col;
		x -= (mgr->settings.center.x ? lw/2. : mgr->settings.offset.x);
	}
	if (*fy) y = 0.; else if (*ly) y = ph - *ih;
	else {
		y = (ah - mgr->settings.overlap.y) * row;
		y -= (mgr->settings.center.y ? lh/2. : mgr->settings.offset.y);
	}
	if (x < 0 || y < 0)
		g_error ("Problem at (%i,%i): x=%.2f, y=%.2f "
			 "(%.2f x %.2f)", col, row, x, y, aw, ah);
	gdk_pixbuf_copy_area (*p, (gint) x, (gint) y,
			      (gint) *iw, (gint) *ih, pixbuf, 0, 0);
	g_object_unref (*p); *p = pixbuf;

	/* Where to place the image? */
	*ix = GPI_MGR (mgr)->settings.margin.left;
	*iy = GPI_MGR (mgr)->settings.margin.top;
	if (*fx) *ix += (mgr->settings.center.x?lw/2.:mgr->settings.offset.x);
	if (*fy) *iy += (mgr->settings.center.y?lh/2.:mgr->settings.offset.y);

#if 0
	g_message ("+-----------------------------------+");
	g_message ("|              Page                 |");
	g_message ("|                                   |");
	g_message ("| Position: (%3i, %3i) in %3i x %3i |", col, row, nx, ny);
	g_message ("| Orig. image: %3i x %3i            |", pw, ph);
	g_message ("| Available: %3.0f x %3.0f              |", aw, ah);
	g_message ("| Leftover: %3.0f (x) and %3.0f (y)     |", lw, lh);
	g_message ("| Image: %3.0f x %3.0f at (%3.0f, %3.0f)    |",
			*iw, *ih, *ix, *iy);
	g_message ("| Last col? %2i                      |", *lx);
	g_message ("| Last row? %2i                      |", *ly);
	g_message ("+-----------------------------------+");
#endif
}

gboolean
gpi_mgr_pixbuf_get_dim (GPIMgrPixbuf *mgr, gdouble *rw, gdouble *rh,
		gdouble *aw, gdouble *ah, guint *pw, guint *ph,
		guint *nx, guint *ny, gdouble *lw, gdouble *lh)
{
	guint w, h;

	g_return_val_if_fail (GPI_IS_MGR_PIXBUF (mgr), FALSE);
	g_return_val_if_fail (aw && ah && pw && ph && rw && rh, FALSE);
	g_return_val_if_fail (nx && ny && lw && lh, FALSE);

	/* Width and height of the image */
	w = gdk_pixbuf_get_width (mgr->pixbuf);
	h = gdk_pixbuf_get_height (mgr->pixbuf);
	g_return_val_if_fail (w && h, FALSE);

	/* Real width and height of the page */
	switch (GPI_MGR (mgr)->settings.orient) {
	case GPI_MGR_SETTINGS_ORIENT_0:
	case GPI_MGR_SETTINGS_ORIENT_180:
		*rw = GPI_MGR (mgr)->settings.page.width;
		*rh = GPI_MGR (mgr)->settings.page.height;
		break;
	default:
		*rw = GPI_MGR (mgr)->settings.page.height;
		*rh = GPI_MGR (mgr)->settings.page.width;
		break;
	}
	g_return_val_if_fail ((*rw > 0) && (*rh > 0), FALSE);

	/* Calculate the available place on the paper */
	*aw = *rw - GPI_MGR (mgr)->settings.margin.left -
		    GPI_MGR (mgr)->settings.margin.right;
	*ah = *rh - GPI_MGR (mgr)->settings.margin.top -
		    GPI_MGR (mgr)->settings.margin.bottom;
	g_return_val_if_fail ((*aw > 0) && (*ah > 0), FALSE);

	/* Calculate the size of the target image */
	if (mgr->settings.fit_to_page) {
		if ((gdouble) h / w > *rh / *rw) {
			*pw = *ah * w / h; *ph = *ah;
		} else { *pw = *aw; *ph = *aw * h / w; }
	} else {
		*pw = w * mgr->settings.adjust_to / 100.;
		*ph = h * mgr->settings.adjust_to / 100.;
	}
	g_return_val_if_fail ((*pw > 0) && (*ph > 0), FALSE);

	/* Number of pages */
	for (*nx=1; *pw > *nx * *aw - (*nx - 1) * mgr->settings.overlap.x - 
		(mgr->settings.center.x ? 0 : mgr->settings.offset.x); (*nx)++);
	for (*ny=1; *ph > *ny * *ah - (*ny - 1) * mgr->settings.overlap.y -
		(mgr->settings.center.y ? 0 : mgr->settings.offset.y); (*ny)++);

	/* Leftover */
	*lw = *nx * *aw - (*nx - 1) * mgr->settings.overlap.x - *pw;
	*lh = *ny * *ah - (*ny - 1) * mgr->settings.overlap.y - *ph;

	return TRUE;
}
