#include <config.h>
#include "gpi-dialog-pixbuf-page.h"

#include <string.h>

#include <libgnomecanvas/gnome-canvas-pixbuf.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>
#include <libgnomecanvas/gnome-canvas-line.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libgnomecanvas/gnome-canvas-text.h>
#include <libgnomecanvas/gnome-canvas-polygon.h>

#define GPI_DIALOG_PIXBUF_PAGE_BORDER_X 5
#define GPI_DIALOG_PIXBUF_PAGE_BORDER_Y 5

#define ORDER_SIZE 20
#define ORDER_OFFSET 5

#define GCI(o) GNOME_CANVAS_ITEM(o)

static GObjectClass *parent_class = NULL;

struct _GPIDialogPixbufPagePrivate {
	GPIMgrPixbuf *mgr;
	guint n;

	GnomeCanvasGroup *group;
	
	GnomeCanvasItem  *bg_white, *bg_black, *image;

	GnomeCanvasItem *margins;

	struct {
		GnomeCanvasGroup *group;
		GnomeCanvasItem *right, *bottom;
	} overlap;

	struct {
		GnomeCanvasGroup *group;
		GnomeCanvasItem  *top_right, *top_left;
		GnomeCanvasItem  *bottom_right, *bottom_left;
		GnomeCanvasItem  *right_bottom, *right_top;
		GnomeCanvasItem  *left_bottom, *left_top;
	} cut;

	struct {
		GnomeCanvasGroup *group;
		GnomeCanvasItem *top, *right, *left, *bottom;
		GnomeCanvasItem *ttop, *tright, *tleft, *tbottom;
		GnomeCanvasItem *number;
	} o;

	gboolean dragging;
	gdouble drag_x, drag_y;
	struct {gdouble x, y;} offset;

	guint update;
	gboolean selected;
};

static void
move_line (GnomeCanvasItem *item,
	   gdouble x1, gdouble y1, gdouble x2, gdouble y2)
{
        GnomeCanvasPoints *points = gnome_canvas_points_new (2);
        points->coords[0] = (double) x1; points->coords [1] = (double) y1;
        points->coords[2] = (double) x2; points->coords [3] = (double) y2;
        g_object_set (item, "points", points, NULL);
        gnome_canvas_points_unref (points);
}

#define OFFSET_X 2
#define OFFSET_Y 2

void
gpi_dialog_pixbuf_page_update_now (GPIDialogPixbufPage *pp)
{
	gdouble aw, ah, lw, lh, rw, rh;
	guint pw, ph, nx, ny, row, col;
	GdkPixbuf *p;
	gboolean fx, lx, fy, ly;
	gdouble ix, iy, iw, ih;
	GnomeCanvas *c;
	gdouble f;
	gchar *s;

	g_return_if_fail (GPI_IS_DIALOG_PIXBUF_PAGE (pp));

	c = GCI (pp->priv->group)->canvas;

	/*
	 * How many pages do we need? First, calculate the available
	 * width and height ('aw' and 'ah'). Then take into account
	 * the overlap.
	 *
	 * rw ... real width (of paper)
	 * rh ... real height (of paper)
	 */
	g_return_if_fail (gpi_mgr_pixbuf_get_dim (pp->priv->mgr,
			&rw, &rh, &aw, &ah, &pw, &ph, &nx, &ny, &lw, &lh));
	p = gdk_pixbuf_scale_simple (pp->priv->mgr->pixbuf, pw, ph,
				     GDK_INTERP_HYPER);

	g_return_if_fail (pp->priv->n < nx * ny);

	f = MIN (((c->scroll_x2 - c->scroll_x1) - (double) nx * (OFFSET_X
		+ 2 * GPI_DIALOG_PIXBUF_PAGE_BORDER_X)) / (double) (nx * rw),
		 ((c->scroll_y2 - c->scroll_y1) - (double) ny * (OFFSET_Y
		+ 2 * GPI_DIALOG_PIXBUF_PAGE_BORDER_Y)) / (double) (ny * rh));
	if (f != c->pixels_per_unit) gnome_canvas_set_pixels_per_unit (c, f);

	/* We are in which column and in which row? */
	if (pp->priv->mgr->settings.down_right) {
		col = pp->priv->n / ny;
		row = pp->priv->n % ny;
	} else {
		row = pp->priv->n / nx;
		col = pp->priv->n % nx;
	}

	gpi_mgr_pixbuf_get_dim2 (pp->priv->mgr,
		aw, ah, pw, ph, nx, ny, lw, lh, col, row,
		&fx, &fy, &lx, &ly, &iw, &ih, &p, &ix, &iy);

	/* Calculate the position of the left upper corner of the page. */
	g_object_set (GCI (pp->priv->group),
		"x", (double) col * rw +
			GPI_DIALOG_PIXBUF_PAGE_BORDER_X / f * (1. + 2. * col),
		"y", (double) row * rh +
			GPI_DIALOG_PIXBUF_PAGE_BORDER_Y / f * (1. + 2. * row),
		NULL);

	g_object_set (pp->priv->image, "pixbuf", p, "x", ix, "y", iy, NULL);
	gdk_pixbuf_unref (p);

	/* Update page. */
	g_object_set (pp->priv->bg_black, 
		"x1", (double) OFFSET_X / f, "y1", (double) + OFFSET_Y / f,
		"x2", (double) rw + OFFSET_X / f,
		"y2", (double) rh + OFFSET_Y / f, NULL);
	g_object_set (pp->priv->bg_white,
		"x1", 0., "y1", 0., "x2", (double) rw, "y2", (double) rh, NULL);

	/* Update margins. */
	if (GPI_MGR (pp->priv->mgr)->settings.margin.print) {
		g_object_set (pp->priv->margins,
		  "x1", GPI_MGR (pp->priv->mgr)->settings.margin.left,
		  "x2", rw - GPI_MGR (pp->priv->mgr)->settings.margin.right,
		  "y1", GPI_MGR (pp->priv->mgr)->settings.margin.top,
		  "y2", rh - GPI_MGR (pp->priv->mgr)->settings.margin.bottom,
		  NULL);
		gnome_canvas_item_raise_to_top (GCI (pp->priv->margins));
		gnome_canvas_item_show (GCI (pp->priv->margins));
	} else gnome_canvas_item_hide (GCI (pp->priv->margins));

	/* Do we have to display the cutting help? */
	if (!pp->priv->mgr->settings.show_cut)
		gnome_canvas_item_hide (GCI (pp->priv->cut.group));
	else {
		gdouble x3, y3;

		x3 = rw - 2. *
		     GPI_MGR (pp->priv->mgr)->settings.margin.right / 3.;
		y3 = rh - 2. *
		     GPI_MGR (pp->priv->mgr)->settings.margin.bottom / 3.;

		move_line (pp->priv->cut.top_left, ix, 0., ix,
			2. * GPI_MGR (pp->priv->mgr)->settings.margin.top / 3.);
		move_line (pp->priv->cut.top_right, ix + iw, 0., ix + iw,
			2. * GPI_MGR (pp->priv->mgr)->settings.margin.top / 3.);
		move_line (pp->priv->cut.bottom_left, ix, rh, ix, y3);
		move_line (pp->priv->cut.bottom_right, ix + iw, rh,
			ix + iw, y3);
		move_line (pp->priv->cut.left_top, 0., iy,
			2. * GPI_MGR (pp->priv->mgr)->settings.margin.left / 3.,
			iy);
		move_line (pp->priv->cut.left_bottom, 0., iy + ih,
			2. * GPI_MGR (pp->priv->mgr)->settings.margin.left / 3.,
			iy + ih);
		move_line (pp->priv->cut.right_top, rw, iy, x3, iy);
		move_line (pp->priv->cut.right_bottom, rw,
			iy + ih, x3, iy + ih);

		gnome_canvas_item_show (GCI (pp->priv->cut.group));
		gnome_canvas_item_raise_to_top (GCI (pp->priv->cut.group));
	}

	/* Overlap?  */
	if (!pp->priv->mgr->settings.show_overlap || lx)
		gnome_canvas_item_hide (GCI (pp->priv->overlap.right));
	if (!pp->priv->mgr->settings.show_overlap || ly)
		gnome_canvas_item_hide (GCI (pp->priv->overlap.bottom));
	if (!pp->priv->mgr->settings.show_overlap)
		gnome_canvas_item_hide (GCI (pp->priv->overlap.group));
	else {
		gnome_canvas_item_show (GCI (pp->priv->overlap.group));
		gnome_canvas_item_raise_to_top (GCI (pp->priv->overlap.group));
	}
	if (pp->priv->mgr->settings.show_overlap && !lx) {
		move_line (pp->priv->overlap.right,
			   ix + iw - pp->priv->mgr->settings.overlap.x, 0.,
			   ix + iw - pp->priv->mgr->settings.overlap.x, rh);
		gnome_canvas_item_show (GCI (pp->priv->overlap.right));
	}
	if (pp->priv->mgr->settings.show_overlap && !ly) {
		move_line (pp->priv->overlap.bottom, 0.,
			iy + ih - pp->priv->mgr->settings.overlap.y, rw,
			iy + ih - pp->priv->mgr->settings.overlap.y);
		gnome_canvas_item_show (GCI (pp->priv->overlap.bottom));
	}

	/* Order? */
	if (pp->priv->mgr->settings.show_order) {
		gint size = ORDER_SIZE * 500 *
			GCI (pp->priv->o.number)->canvas->pixels_per_unit;
		gboolean dr = pp->priv->mgr->settings.down_right;

		/* Font size. Patches welcome. */
		g_object_set (pp->priv->o.tleft, "size", size, NULL);
		g_object_set (pp->priv->o.tright, "size", size, NULL);
		g_object_set (pp->priv->o.ttop, "size", size, NULL);
		g_object_set (pp->priv->o.tbottom, "size", size, NULL);

		/* Left */
		if (!fx) {
		  if (dr) s = g_strdup_printf ("%i", pp->priv->n - ny + 1);
		  else s = g_strdup_printf ("%i", pp->priv->n); 
		  g_object_set (pp->priv->o.tleft, "text", s, NULL);
		  g_free (s);
		}

		/* Right */
		if (!lx) {
		  if (dr) s = g_strdup_printf ("%i", pp->priv->n + ny + 1);
		  else s = g_strdup_printf ("%i", pp->priv->n + 2);
		  g_object_set (pp->priv->o.tright, "text", s, NULL);
		  g_free (s);
		}

		/* Top */
		if (!fy) {
		  if (dr) s = g_strdup_printf ("%i", pp->priv->n);
		  else s = g_strdup_printf ("%i", pp->priv->n - nx + 1);
		  g_object_set (pp->priv->o.ttop, "text", s, NULL);
		  g_free (s);
		}

		/* Bottom */
		if (!ly) {
		  if (dr) s = g_strdup_printf ("%i", pp->priv->n + 2);
		  else s = g_strdup_printf ("%i", pp->priv->n + nx + 1);
		  g_object_set (pp->priv->o.tbottom, "text", s, NULL);
		  g_free (s);
		}

		s = g_strdup_printf ("%i/%i", pp->priv->n + 1, nx * ny);
		g_object_set (pp->priv->o.number, "size", size,
			"text", s, "x", (gdouble) rw + ORDER_OFFSET -
			GPI_MGR (pp->priv->mgr)->settings.margin.right,
			"y", (gdouble) rh + ORDER_OFFSET - 
			GPI_MGR (pp->priv->mgr)->settings.margin.bottom, NULL);
		g_free (s);

		if (!fx) {
			g_object_set (GCI (pp->priv->o.left),
				"x", (gdouble) GPI_MGR (
				pp->priv->mgr)->settings.margin.left -
				ORDER_SIZE - ORDER_OFFSET,
				"y", (gdouble) (rh - ORDER_SIZE) / 2., NULL);
			gnome_canvas_item_show (GCI (pp->priv->o.left));
		} else gnome_canvas_item_hide (GCI (pp->priv->o.left));
		if (!lx) {
			g_object_set (GCI (pp->priv->o.right),
				"x", (gdouble) rw + ORDER_OFFSET -
				GPI_MGR (pp->priv->mgr)->settings.margin.right,
				"y", (gdouble) (rh - ORDER_SIZE) / 2., NULL);
			gnome_canvas_item_show (GCI (pp->priv->o.right));
		} else gnome_canvas_item_hide (GCI (pp->priv->o.right));
		if (!fy) {
			g_object_set (GCI (pp->priv->o.top),
				"x", (gdouble) (rw - ORDER_SIZE) / 2.,
				"y", 
				GPI_MGR (pp->priv->mgr)->settings.margin.top -
				ORDER_SIZE - ORDER_OFFSET, NULL);
			gnome_canvas_item_show (GCI (pp->priv->o.top));
		} else gnome_canvas_item_hide (GCI (pp->priv->o.top));
		if (!ly) {
			g_object_set (GCI (pp->priv->o.bottom),
			    "x", (gdouble) (rw - ORDER_SIZE) / 2.,
			    "y", rh + ORDER_OFFSET -
			    GPI_MGR (pp->priv->mgr)->settings.margin.bottom,
			    NULL);
			gnome_canvas_item_show (GCI (pp->priv->o.bottom));
		} else gnome_canvas_item_hide (GCI (pp->priv->o.bottom));
		g_object_set (GCI (pp->priv->o.number),
			"x", (gdouble) rw -
			GPI_MGR (pp->priv->mgr)->settings.margin.right,
			"y", (gdouble) rh - MAX (ORDER_SIZE,
			GPI_MGR (pp->priv->mgr)->settings.margin.bottom),
			NULL);
		gnome_canvas_item_show (GCI (pp->priv->o.group));
		gnome_canvas_item_raise_to_top (GCI (pp->priv->o.group));
	} else gnome_canvas_item_hide (GCI (pp->priv->o.group));
}

static gboolean
update_func (gpointer data)
{
	GPIDialogPixbufPage *pp = GPI_DIALOG_PIXBUF_PAGE (data);

	gpi_dialog_pixbuf_page_update_now (pp);
	pp->priv->update = 0;

	return FALSE;
}

void
gpi_dialog_pixbuf_page_update (GPIDialogPixbufPage *pp)
{
	g_return_if_fail (GPI_IS_DIALOG_PIXBUF_PAGE (pp));

	if (pp->priv->update) return;
	pp->priv->update = g_idle_add (update_func, pp);
}

static void
gpi_dialog_pixbuf_page_finalize (GObject *object)
{
	GPIDialogPixbufPage *pp = GPI_DIALOG_PIXBUF_PAGE (object);

	if (pp->priv->update) g_source_remove (pp->priv->update);

	if (GTK_IS_OBJECT (pp->priv->group)) {
		gtk_object_destroy (GTK_OBJECT (pp->priv->group));
		pp->priv->group = NULL;
	}
	if (pp->priv->mgr) {
		g_object_unref (pp->priv->mgr);
		pp->priv->mgr = NULL;
	}

	g_free (pp->priv);

	G_OBJECT_CLASS (parent_class)-> finalize (object);
}

static void
gpi_dialog_pixbuf_page_init (GTypeInstance *instance, gpointer g_class)
{
	GPIDialogPixbufPage *pp = GPI_DIALOG_PIXBUF_PAGE (instance);

	pp->priv = g_new0 (GPIDialogPixbufPagePrivate, 1);
}

static void
gpi_dialog_pixbuf_page_class_init (gpointer g_class, gpointer class_data)
{
	GObjectClass *gobject_class;
	
	gobject_class = G_OBJECT_CLASS (g_class);
	gobject_class->finalize = gpi_dialog_pixbuf_page_finalize;

	parent_class = g_type_class_peek_parent (g_class);
}

static GnomeCanvasItem *
make_line (GnomeCanvasGroup *group, const gchar *color)
{ 
	GnomeCanvasPoints *p = gnome_canvas_points_new (2);
	GnomeCanvasItem *i; 
	
	p->coords[0] = 0.0; p->coords [1] = 0.0; 
	p->coords[2] = 0.0; p->coords [3] = 0.0; 
	i = gnome_canvas_item_new (group, gnome_canvas_line_get_type (), 
		"points", p, "width_pixels", 1, "fill_color", color, NULL);
	gnome_canvas_points_unref (p); 

	return i;
}

static void
on_destroy (GtkObject *o, GPIDialogPixbufPage *pp)
{
	g_return_if_fail (GPI_IS_DIALOG_PIXBUF_PAGE (pp));

	pp->priv->group = NULL;
}

static gint
on_event (GnomeCanvasItem *item, GdkEvent *event, GPIDialogPixbufPage *pp)
{
	GdkCursor *cursor;
	gdouble aw, ah, lw, lh, rw, rh;
	guint pw, ph, nx, ny;
	GnomeCanvas *c = GCI (pp->priv->group)->canvas;
	guint i;

	gpi_mgr_pixbuf_get_dim (pp->priv->mgr, &rw, &rh,
				&aw, &ah, &pw, &ph, &nx, &ny, &lw, &lh);
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (event->button.button) {
		case 1:
			pp->priv->dragging = TRUE;
			if (pp->priv->mgr->settings.center.x)
				pp->priv->mgr->settings.offset.x = lw / 2.;
			if (pp->priv->mgr->settings.center.y)
				pp->priv->mgr->settings.offset.y = lh / 2.;
			pp->priv->mgr->settings.center.x = FALSE;
			pp->priv->mgr->settings.center.y = FALSE;
			g_signal_emit_by_name (pp->priv->mgr,
					       "settings_changed");
			pp->priv->drag_x = event->button.x;
			pp->priv->drag_y = event->button.y;
			pp->priv->offset.x = pp->priv->mgr->settings.offset.x;
			pp->priv->offset.y = pp->priv->mgr->settings.offset.y;
			cursor = gdk_cursor_new (GDK_FLEUR);
			gnome_canvas_item_grab (item,
				(GDK_POINTER_MOTION_MASK |
				 GDK_BUTTON_RELEASE_MASK), cursor,
				event->button.time);
			gdk_cursor_unref (cursor);
			break;
		default:
			break;
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (pp->priv->dragging &&
		    (event->motion.state & GDK_BUTTON1_MASK)) {
			pp->priv->mgr->settings.offset.x = pp->priv->offset.x -
				(pp->priv->drag_x - event->button.x) /
				c->pixels_per_unit;
			pp->priv->mgr->settings.offset.y = pp->priv->offset.y -
				(pp->priv->drag_y - event->button.y) /
				c->pixels_per_unit;
			pp->priv->mgr->settings.offset.x =
				MAX (0, pp->priv->mgr->settings.offset.x);
			pp->priv->mgr->settings.offset.y =
				MAX (0, pp->priv->mgr->settings.offset.y);
			pp->priv->mgr->settings.offset.x =
				MIN (lw, pp->priv->mgr->settings.offset.x);
			pp->priv->mgr->settings.offset.y =
				MIN (lh, pp->priv->mgr->settings.offset.y);
			g_signal_emit_by_name (pp->priv->mgr,
					       "settings_changed");
		}
		break;
	case GDK_BUTTON_RELEASE:
		gnome_canvas_item_ungrab (item, event->button.time);
		pp->priv->dragging = FALSE;
		if ((pp->priv->drag_x == event->button.x) &&
		    (pp->priv->drag_y == event->button.y)) {
			GArray *s = GPI_MGR (pp->priv->mgr)->selection;

			pp->priv->selected = !pp->priv->selected;
			g_object_set (pp->priv->bg_black,
			    "fill_color"   , pp->priv->selected?"red":"black",
			    "outline_color", pp->priv->selected?"red":"black",
			    NULL);
			for (i = 0; i < s->len; i++)
				if (pp->priv->n >=
					g_array_index (s, guint, i)) break;
			if (!pp->priv->selected &&
			    (pp->priv->n == g_array_index (s, guint, i)))
				g_array_remove_index (s, i);
			if (pp->priv->selected &&
			    ((i == s->len) ||
			     (pp->priv->n != g_array_index (s, guint, i))))
				g_array_insert_val (s, i, pp->priv->n);
		}
		break;
	default:
		break;
	}

	return FALSE;
}

GPIDialogPixbufPage *
gpi_dialog_pixbuf_page_new (GPIMgrPixbuf *mgr, GnomeCanvasGroup *group, guint n)
{
	GPIDialogPixbufPage *pp;
	GnomeCanvasItem *g;
	GnomeCanvasPoints *p;

	g_return_val_if_fail (GPI_IS_MGR_PIXBUF (mgr), NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_GROUP (group), NULL);

	pp = g_object_new (GPI_TYPE_DIALOG_PIXBUF_PAGE, NULL);
	pp->priv->mgr = g_object_ref (mgr);
	pp->priv->n = n;

	/*
	 * We put all items in one group in order to be able to destroy
	 * all of them at once.
	 */
	g = gnome_canvas_item_new (group, GNOME_TYPE_CANVAS_GROUP, "x", 0.,
				   "y", 0., NULL);
	pp->priv->group = GNOME_CANVAS_GROUP (g);
	g_object_ref (pp->priv->group);
	g_signal_connect (g, "destroy", G_CALLBACK (on_destroy), pp);

	pp->priv->image = gnome_canvas_item_new (pp->priv->group,
		GNOME_TYPE_CANVAS_PIXBUF, "anchor", GTK_ANCHOR_NW, NULL);
	g_assert (pp->priv->image);
	g_signal_connect (pp->priv->image, "event", G_CALLBACK (on_event), pp);

	pp->priv->bg_black = gnome_canvas_item_new (
		pp->priv->group, GNOME_TYPE_CANVAS_RECT, "fill_color", "black",
		"outline_color", "black", "width_pixels", 1, NULL);
	pp->priv->bg_white = gnome_canvas_item_new (
		pp->priv->group, GNOME_TYPE_CANVAS_RECT, "fill_color", "white",
		"outline_color", "white", "width_pixels", 1, NULL);
	gnome_canvas_item_raise_to_top (pp->priv->image);

	/* Margins */
	pp->priv->margins = gnome_canvas_item_new (pp->priv->group,
		GNOME_TYPE_CANVAS_RECT, "outline_color", "grey70", NULL);

	/* Overlap */
	g = gnome_canvas_item_new (pp->priv->group,
		GNOME_TYPE_CANVAS_GROUP, "x", 0.0, "y", 0.0, NULL);
	pp->priv->overlap.group = GNOME_CANVAS_GROUP (g);
	pp->priv->overlap.right = make_line (pp->priv->overlap.group, "black");
	pp->priv->overlap.bottom = make_line (pp->priv->overlap.group, "black");

	/* Ordering help */
	g = gnome_canvas_item_new (pp->priv->group,
		GNOME_TYPE_CANVAS_GROUP, NULL);
	pp->priv->o.group = GNOME_CANVAS_GROUP (g);
	pp->priv->o.right = gnome_canvas_item_new (pp->priv->o.group,
		GNOME_TYPE_CANVAS_GROUP, NULL);
	pp->priv->o.bottom = gnome_canvas_item_new (pp->priv->o.group,
		GNOME_TYPE_CANVAS_GROUP, NULL);
	pp->priv->o.left = gnome_canvas_item_new (pp->priv->o.group,
		GNOME_TYPE_CANVAS_GROUP, NULL);
	pp->priv->o.top = gnome_canvas_item_new (pp->priv->o.group,
		GNOME_TYPE_CANVAS_GROUP, NULL);
	p = gnome_canvas_points_new (5);
	memset (p->coords, 0, sizeof (p->coords[0]) * p->num_points * 2);
	p->coords[2] = ORDER_SIZE / 2.;
	p->coords[4] = ORDER_SIZE;
	p->coords[5] = ORDER_SIZE / 2.;
	p->coords[6] = ORDER_SIZE / 2.;
	p->coords[7] = ORDER_SIZE;
	p->coords[9] = ORDER_SIZE;
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (pp->priv->o.right),
		GNOME_TYPE_CANVAS_POLYGON, "points", p,
		"outline_color", "blue", NULL);
	gnome_canvas_points_unref (p);
	p = gnome_canvas_points_new (5);
	memset (p->coords, 0, sizeof (p->coords[0]) * p->num_points * 2);
	p->coords[2] = ORDER_SIZE;
	p->coords[4] = ORDER_SIZE;
	p->coords[5] = ORDER_SIZE / 2.;
	p->coords[6] = ORDER_SIZE / 2.;
	p->coords[7] = ORDER_SIZE;
	p->coords[9] = ORDER_SIZE / 2.;
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (pp->priv->o.bottom),
		GNOME_TYPE_CANVAS_POLYGON, "points", p,
		"outline_color", "blue", NULL);
	gnome_canvas_points_unref (p);
	p = gnome_canvas_points_new (5);
	memset (p->coords, 0, sizeof (p->coords[0]) * p->num_points * 2);
	p->coords[0] = ORDER_SIZE / 2.;
	p->coords[2] = ORDER_SIZE;
	p->coords[3] = ORDER_SIZE / 2.;
	p->coords[4] = ORDER_SIZE;
	p->coords[5] = ORDER_SIZE;
	p->coords[7] = ORDER_SIZE;
	p->coords[9] = ORDER_SIZE / 2.;
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (pp->priv->o.top),
		GNOME_TYPE_CANVAS_POLYGON, "points", p,
		"outline_color", "blue", NULL);
	gnome_canvas_points_unref (p);
	p = gnome_canvas_points_new (5);
	memset (p->coords, 0, sizeof (p->coords[0]) * p->num_points * 2);
	p->coords[1] = ORDER_SIZE / 2.;
	p->coords[2] = ORDER_SIZE / 2.;
	p->coords[4] = ORDER_SIZE;
	p->coords[6] = ORDER_SIZE;
	p->coords[7] = ORDER_SIZE;
	p->coords[8] = ORDER_SIZE / 2.;
	p->coords[9] = ORDER_SIZE;
	gnome_canvas_item_new (GNOME_CANVAS_GROUP (pp->priv->o.left),
		GNOME_TYPE_CANVAS_POLYGON, "points", p,
		"outline_color", "blue", NULL);
	gnome_canvas_points_unref (p);
	pp->priv->o.tright = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (pp->priv->o.right),
		GNOME_TYPE_CANVAS_TEXT, "fill_color", "blue",
		"anchor", GTK_ANCHOR_CENTER,
		"x", (gdouble) ORDER_SIZE / 3., 
		"y", (gdouble) ORDER_SIZE / 2., NULL);
	pp->priv->o.tleft = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (pp->priv->o.left),
		GNOME_TYPE_CANVAS_TEXT, "fill_color", "blue",
		"anchor", GTK_ANCHOR_CENTER,
		"x", (gdouble) ORDER_SIZE * 2 / 3.,
		"y", (gdouble) ORDER_SIZE / 2., NULL);
	pp->priv->o.ttop = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (pp->priv->o.top),
		GNOME_TYPE_CANVAS_TEXT, "fill_color", "blue",
		"x", (gdouble) ORDER_SIZE / 2.,
		"y", (gdouble) ORDER_SIZE * 2 / 3., NULL);
	pp->priv->o.tbottom = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (pp->priv->o.bottom),
		GNOME_TYPE_CANVAS_TEXT, "fill_color", "blue",
		"x", (gdouble) ORDER_SIZE / 2.,
		"y", (gdouble) ORDER_SIZE / 3., NULL);
	pp->priv->o.number = gnome_canvas_item_new (pp->priv->o.group,
		GNOME_TYPE_CANVAS_TEXT, "fill_color", "blue",
		"anchor", GTK_ANCHOR_NORTH_WEST, NULL);

	/* Cutting help */
	g = gnome_canvas_item_new (pp->priv->group,
		GNOME_TYPE_CANVAS_GROUP, "x", 0., "y", 0., NULL);
	pp->priv->cut.group = GNOME_CANVAS_GROUP (g);
	pp->priv->cut.left_top = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.left_bottom = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.right_top = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.right_bottom = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.top_right = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.top_left = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.bottom_right = make_line (pp->priv->cut.group, "black");
	pp->priv->cut.bottom_left = make_line (pp->priv->cut.group, "black");

	return pp;
}

GType
gpi_dialog_pixbuf_page_get_type (void)
{
	static GType t = 0;

	if (!t) {
		static const GTypeInfo ti = {
			sizeof (GPIDialogPixbufPageClass),
			NULL, NULL, gpi_dialog_pixbuf_page_class_init,
			NULL, NULL, sizeof (GPIDialogPixbufPage), 0,
			gpi_dialog_pixbuf_page_init, NULL,
		};
		t = g_type_register_static (G_TYPE_OBJECT, 
			"GPIDialogPixbufPage", &ti, 0);
	}

	return t;
}
