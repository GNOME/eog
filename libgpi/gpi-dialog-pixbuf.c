#include <config.h>
#include "gpi-dialog-pixbuf.h"
#include "gpi-mgr-pixbuf.h"
#include "gpi-dialog-pixbuf-page.h"

#include <gtk/gtklabel.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>

#include <glade/glade-xml.h>

#define _(s) (s)

struct _GPIDialogPixbufPrivate {
	GPtrArray *pages;
	GladeXML *xml;
};

static GPIDialogClass *parent_class = NULL;

static void
gpi_dialog_pixbuf_set_number_of_pages (GPIDialogPixbuf *d, guint n)
{
	g_return_if_fail (GPI_IS_DIALOG_PIXBUF (d));

	while (d->priv->pages->len > n) {
		g_object_unref (d->priv->pages->pdata[d->priv->pages->len - 1]);
		g_ptr_array_remove_index (d->priv->pages,
					  d->priv->pages->len - 1);
	}
	while (d->priv->pages->len < n)
		g_ptr_array_add (d->priv->pages, gpi_dialog_pixbuf_page_new (
			GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr),
			gnome_canvas_root (GNOME_CANVAS (
				glade_xml_get_widget (d->priv->xml,
				"canvas_preview"))), d->priv->pages->len));
}

static void
gpi_dialog_pixbuf_finalize (GObject *object)
{
	GPIDialogPixbuf *d = GPI_DIALOG_PIXBUF (object);

	gpi_dialog_pixbuf_set_number_of_pages (d, 0);
	g_ptr_array_free (d->priv->pages, TRUE);
	g_object_unref (d->priv->xml);
	g_free (d->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gpi_dialog_pixbuf_class_init (gpointer klass, gpointer class_data)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = gpi_dialog_pixbuf_finalize;

	parent_class = g_type_class_peek_parent (klass);
}

static void
gpi_dialog_pixbuf_instance_init (GTypeInstance *instance, gpointer klass)
{
	GPIDialogPixbuf *d = GPI_DIALOG_PIXBUF (instance);

	d->priv = g_new0 (GPIDialogPixbufPrivate, 1);
	d->priv->pages = g_ptr_array_new ();
}

GType
gpi_dialog_pixbuf_get_type (void)
{
	static GType t = 0;

	if (!t) {
		static const GTypeInfo i = {
			sizeof (GPIDialogPixbufClass), NULL, NULL,
			gpi_dialog_pixbuf_class_init, NULL, NULL,
			sizeof (GPIDialogPixbuf), 0,
			gpi_dialog_pixbuf_instance_init
		};

		t = g_type_register_static (GPI_TYPE_DIALOG, "GPIDialogPixbuf",
					    &i, 0);
	}

	return t;
}

static void
gpi_dialog_pixbuf_update (GPIDialogPixbuf *d)
{
        guint nx, ny, i;
        guint pw, ph;
        gdouble aw, ah, lw, lh, rw, rh;

        g_return_if_fail (GPI_IS_DIALOG_PIXBUF (d));
                                                                                
	g_return_if_fail (gpi_mgr_pixbuf_get_dim (
		GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr), &rw, &rh,
		&aw, &ah, &pw, &ph, &nx, &ny, &lw, &lh));
	gpi_dialog_pixbuf_set_number_of_pages (d, nx * ny);
                                                                                
        /* Update all pages */
        for (i = 0; i < d->priv->pages->len; i++)
                gpi_dialog_pixbuf_page_update (GPI_DIALOG_PIXBUF_PAGE (
                                        d->priv->pages->pdata[i]));
}

static void
on_size_allocate (GtkWidget *widget, GtkAllocation *a, GPIDialogPixbuf *d)
{
	guint i;
	GnomeCanvas *c = GNOME_CANVAS (glade_xml_get_widget (d->priv->xml,
							"canvas_preview"));

	gnome_canvas_set_scroll_region (c, 0.0, 0.0, a->width, a->height);
	gnome_canvas_set_center_scroll_region (c, FALSE);
	for (i = 0; i < d->priv->pages->len; i++)
		gpi_dialog_pixbuf_page_update (
			GPI_DIALOG_PIXBUF_PAGE (d->priv->pages->pdata[i]));
}

static void
on_center_y_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.center.y != t->active) {
		mgr->settings.center.y = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_center_x_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.center.x != t->active) {
		mgr->settings.center.x = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_overlap_help_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.show_overlap != t->active) {
		mgr->settings.show_overlap = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_ordering_help_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.show_order != t->active) {
		mgr->settings.show_order = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_print_margins_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (GPI_MGR (mgr)->settings.margin.print != t->active) {
		GPI_MGR (mgr)->settings.margin.print = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_cutting_help_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.show_cut != t->active) {
		mgr->settings.show_cut = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_adjust_to_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.fit_to_page != !t->active) {
		mgr->settings.fit_to_page = !t->active;
		gtk_widget_set_sensitive (glade_xml_get_widget (d->priv->xml,
					"spinbutton_adjust_to"), t->active);
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_fit_to_page_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.fit_to_page != t->active) {
		mgr->settings.fit_to_page = t->active;
		gtk_widget_set_sensitive (glade_xml_get_widget (d->priv->xml,
					"spinbutton_adjust_to"), !t->active);
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_down_right_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.down_right != t->active) {
		mgr->settings.down_right = t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_right_down_toggled (GtkToggleButton *t, GPIDialogPixbuf *d)
{
	GPIMgrPixbuf *mgr = GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr);

	if (mgr->settings.down_right != !t->active) {
		mgr->settings.down_right = !t->active;
		g_signal_emit_by_name (mgr, "settings_changed");
	}
}

static void
on_adjust_to_value_changed (GtkAdjustment *a, GPIDialogPixbuf *d)
{
	GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr)->settings.adjust_to = a->value;
	g_signal_emit_by_name (GPI_DIALOG (d)->mgr, "settings_changed");
}

static void
on_overlap_x_value_changed (GtkAdjustment *a, GPIDialogPixbuf *d)
{
	GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr)->settings.overlap.x = a->value;
	g_signal_emit_by_name (GPI_DIALOG (d)->mgr, "settings_changed");
}

static void
on_overlap_y_value_changed (GtkAdjustment *a, GPIDialogPixbuf *d)
{
	GPI_MGR_PIXBUF (GPI_DIALOG (d)->mgr)->settings.overlap.y = a->value;
	g_signal_emit_by_name (GPI_DIALOG (d)->mgr, "settings_changed");
}

static void
on_settings_changed (GPIMgr *mgr, GPIDialogPixbuf *d)
{
	GtkWidget *w;

	w = glade_xml_get_widget (d->priv->xml, "checkbutton_center_x");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      GPI_MGR_PIXBUF (mgr)->settings.center.x);
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_center_y");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      GPI_MGR_PIXBUF (mgr)->settings.center.y);

	gpi_dialog_pixbuf_update (d);
}

GPIDialogPixbuf *
gpi_dialog_pixbuf_new (GdkPixbuf *pixbuf)
{
	GPIDialogPixbuf *d;
	GPIMgrPixbuf *mgr;
	GtkWidget *w, *l;

	d = g_object_new (GPI_TYPE_DIALOG_PIXBUF, NULL);
	d->priv->xml = glade_xml_new (GLADE_DIR "/gpi-dialog.glade",
				      "table-pixbuf", NULL);
	if (!d->priv->xml)
		d->priv->xml = glade_xml_new (TOP_SRCDIR
			"/libgpi/gpi-dialog.glade", "table-pixbuf", NULL);
	if (!d->priv->xml) {
		g_warning ("UI description not found. Please check your "
			   "setup.");
		g_object_unref (d);
		return NULL;
	}

	mgr = gpi_mgr_pixbuf_new (pixbuf);
	gpi_mgr_load_settings (GPI_MGR (mgr));
	gpi_dialog_construct (GPI_DIALOG (d), GPI_MGR (mgr));
	g_signal_connect (mgr, "settings_changed",
			  G_CALLBACK (on_settings_changed), d);
	g_object_unref (mgr);

	gtk_widget_show (l = gtk_label_new (_("Page")));
	gtk_widget_show (w = glade_xml_get_widget (d->priv->xml,
						   "table-pixbuf"));
	gtk_notebook_append_page (GPI_DIALOG (d)->notebook, w, l);

	/* Resizing the widget */
	g_signal_connect (glade_xml_get_widget (d->priv->xml, "canvas_preview"),
		"size_allocate", G_CALLBACK (on_size_allocate), d);
	gtk_widget_set_size_request (
		glade_xml_get_widget (d->priv->xml, "canvas_preview"),
		100, 100);

	/* Checkbutton */
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_center_x");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      mgr->settings.center.x);
	g_signal_connect (w, "toggled", G_CALLBACK (on_center_x_toggled), d);
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_center_y");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
			              mgr->settings.center.y);
	g_signal_connect (w, "toggled", G_CALLBACK (on_center_y_toggled), d);
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_overlap_help");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
			              mgr->settings.show_overlap);
	g_signal_connect (w, "toggled", G_CALLBACK (on_overlap_help_toggled),
			  d);
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_cutting_help");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      mgr->settings.show_cut);
	g_signal_connect (w, "toggled", G_CALLBACK (on_cutting_help_toggled),
			  d);
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_order_help");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      mgr->settings.show_order);
	g_signal_connect (w, "toggled", G_CALLBACK (on_ordering_help_toggled),
			  d);
	w = glade_xml_get_widget (d->priv->xml, "checkbutton_print_margins");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      GPI_MGR (mgr)->settings.margin.print);
	g_signal_connect (w, "toggled", G_CALLBACK (on_print_margins_toggled),
			  d);

	/* Radiobutton */
	w = glade_xml_get_widget (d->priv->xml, "radiobutton_down_right");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      mgr->settings.down_right);
	g_signal_connect (w, "toggled", G_CALLBACK (on_down_right_toggled), d);
	w = glade_xml_get_widget (d->priv->xml, "radiobutton_right_down");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      !mgr->settings.down_right);
	g_signal_connect (w, "toggled", G_CALLBACK (on_right_down_toggled), d);
	w = glade_xml_get_widget (d->priv->xml, "radiobutton_adjust_to");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      !mgr->settings.fit_to_page);
	g_signal_connect (w, "toggled", G_CALLBACK (on_adjust_to_toggled), d);
	w = glade_xml_get_widget (d->priv->xml, "radiobutton_fit_to_page");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
				      mgr->settings.fit_to_page);
	g_signal_connect (w, "toggled", G_CALLBACK (on_fit_to_page_toggled), d);

	/* Spinbutton */
	w = glade_xml_get_widget (d->priv->xml, "spinbutton_adjust_to");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
				   mgr->settings.adjust_to);
	gtk_widget_set_sensitive (w, !mgr->settings.fit_to_page);
	g_signal_connect (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
		"value_changed", G_CALLBACK (on_adjust_to_value_changed), d);
	w = glade_xml_get_widget (d->priv->xml, "spinbutton_overlap_x");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
				   mgr->settings.overlap.x);
	g_signal_connect (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
		"value_changed", G_CALLBACK (on_overlap_x_value_changed), d);
	w = glade_xml_get_widget (d->priv->xml, "spinbutton_overlap_y");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (w),
				   mgr->settings.overlap.y);
	g_signal_connect (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
		"value_changed", G_CALLBACK (on_overlap_y_value_changed), d);

	/* Update the widget for the first time. */
	gpi_dialog_pixbuf_update (d);

	return d;
}
