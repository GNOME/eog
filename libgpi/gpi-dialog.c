#include <config.h>
#include "gpi-dialog.h"

#include <string.h>

#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>

#include <libgnomeprintui/gnome-print-paper-selector.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#define _(s) (s)

enum {
	OK_CLICKED,
	CANCEL_CLICKED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _GPIDialogPrivate {
	GnomePrintConfig *config;
};

static GtkDialogClass *parent_class = NULL;

static void gpi_dialog_check_paper (GPIDialog *);

static void
gpi_dialog_finalize (GObject *object)
{
	GPIDialog *d = GPI_DIALOG (object);

	g_object_unref (d->priv->config);
	g_object_unref (d->mgr);

	g_free (d->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gpi_dialog_class_init (gpointer klass, gpointer class_data)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = gpi_dialog_finalize;

	signals[OK_CLICKED] = g_signal_new ("ok_clicked",
		G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GPIDialogClass, ok_clicked),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	signals[CANCEL_CLICKED] = g_signal_new ("cancel_clicked",
		G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GPIDialogClass, cancel_clicked),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	parent_class = g_type_class_peek_parent (klass);
}

static void
gpi_dialog_instance_init (GTypeInstance *instance, gpointer klass)
{
	GPIDialog *d = GPI_DIALOG (instance);

	d->priv = g_new0 (GPIDialogPrivate, 1);
}

GType
gpi_dialog_get_type (void)
{
	static GType t = 0;

	if (!t) {
		static const GTypeInfo i = {
			sizeof (GPIDialogClass), NULL, NULL,
			gpi_dialog_class_init, NULL, NULL, sizeof (GPIDialog),
			0, gpi_dialog_instance_init
		};

		t = g_type_register_static (GTK_TYPE_DIALOG, "GPIDialog", &i,
					    G_TYPE_FLAG_ABSTRACT);
	}

	return t;
}

static void
on_cancel_clicked (GtkButton *button, GPIDialog *d)
{
	g_signal_emit_by_name (d, "cancel_clicked");
}

static void
on_ok_clicked (GtkButton *button, GPIDialog *d)
{
	gpi_dialog_check_paper (d);
	gpi_mgr_save_settings (d->mgr);
	g_signal_emit_by_name (d, "ok_clicked");
}

static void
on_print_clicked (GtkButton *button, GPIDialog *d)
{
	GnomePrintJob *job;

	gpi_dialog_check_paper (d);
	job = gpi_mgr_get_job (d->mgr);
	gnome_print_job_print (job);
	g_object_unref (job);
}

static void
on_print_preview_clicked (GtkButton *button, GPIDialog *d)
{
	GnomePrintJob *job;
	GtkWidget *p;

	gpi_dialog_check_paper (d);
	job = gpi_mgr_get_job (d->mgr);
	p = gnome_print_job_preview_new (job, _("Preview"));
	g_object_unref (job);
	gtk_window_set_transient_for (GTK_WINDOW (p), GTK_WINDOW (d));
	gtk_widget_show (p);
}

static void
gpi_dialog_check_paper (GPIDialog *d)
{
	gdouble v_double;
	guchar *v_string;
	gboolean b = FALSE;
	const GnomePrintUnit *u_to, *u;

	g_return_if_fail (GPI_IS_DIALOG (d));

	u_to = gnome_print_unit_get_by_name (_("Millimeter"));
	gnome_print_config_get_length (d->priv->config,
		GNOME_PRINT_KEY_PAPER_WIDTH, &v_double, &u);
	gnome_print_convert_distance (&v_double, u, u_to);
	b = b || (d->mgr->settings.page.width != v_double);
	d->mgr->settings.page.width = v_double;
	gnome_print_config_get_length (d->priv->config,
		GNOME_PRINT_KEY_PAPER_HEIGHT, &v_double, &u);
	gnome_print_convert_distance (&v_double, u, u_to);
	b = b || (d->mgr->settings.page.height != v_double);
	d->mgr->settings.page.height = v_double;
	v_string = gnome_print_config_get (d->priv->config,
		GNOME_PRINT_KEY_ORIENTATION);
	if (v_string) {
		if (!strcmp (v_string, "R90") &&
		    (d->mgr->settings.orient != GPI_MGR_SETTINGS_ORIENT_90)) {
			b = TRUE;
			d->mgr->settings.orient = GPI_MGR_SETTINGS_ORIENT_90;
		} else if (!strcmp (v_string, "R180") && 
		    (d->mgr->settings.orient != GPI_MGR_SETTINGS_ORIENT_180)) {
			b = TRUE;
			d->mgr->settings.orient = GPI_MGR_SETTINGS_ORIENT_180;
		} else if (!strcmp (v_string, "R270") &&
		    (d->mgr->settings.orient != GPI_MGR_SETTINGS_ORIENT_270)) {
			b = TRUE;
			d->mgr->settings.orient = GPI_MGR_SETTINGS_ORIENT_270;
		} else if (!strcmp (v_string, "R0") &&
		    (d->mgr->settings.orient != GPI_MGR_SETTINGS_ORIENT_0)) {
			b = TRUE;
			d->mgr->settings.orient = GPI_MGR_SETTINGS_ORIENT_0;
		}
	}
	gnome_print_config_get_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_LEFT, &v_double, &u);
	gnome_print_convert_distance (&v_double, u, u_to);
	b = b || (d->mgr->settings.margin.left != v_double);
	d->mgr->settings.margin.left = v_double;
	gnome_print_config_get_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_RIGHT, &v_double, &u);
	gnome_print_convert_distance (&v_double, u, u_to);
	b = b || (d->mgr->settings.margin.right != v_double);
	d->mgr->settings.margin.right = v_double;
	gnome_print_config_get_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_TOP, &v_double, &u);
	gnome_print_convert_distance (&v_double, u, u_to);
	b = b || (d->mgr->settings.margin.top != v_double);
	d->mgr->settings.margin.top = v_double;
	gnome_print_config_get_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_BOTTOM, &v_double, &u);
	gnome_print_convert_distance (&v_double, u, u_to);
	b = b || (d->mgr->settings.margin.bottom != v_double);
	d->mgr->settings.margin.bottom = v_double;

	if (b) g_signal_emit_by_name (d->mgr, "settings_changed");
}

void
on_switch_page (GtkNotebook *notebook, GtkNotebookPage *page, guint n,
		GPIDialog *d)
{
	gpi_dialog_check_paper (d);
}

void
gpi_dialog_construct (GPIDialog *d, GPIMgr *mgr)
{
	GtkWidget *b, *hp, *l, *ps;
	const GnomePrintUnit *u;

	g_return_if_fail (GPI_IS_DIALOG (d));
	g_return_if_fail (GPI_IS_MGR (mgr));

	g_object_ref (d->mgr = mgr);

	/* Basic layout */
	gtk_widget_show (hp = gtk_hbox_new (FALSE, 5));
	gtk_container_set_border_width (GTK_CONTAINER (hp), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (d)->vbox), hp, TRUE, TRUE, 0);
	d->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_widget_show (GTK_WIDGET (d->notebook));
	gtk_box_pack_start (GTK_BOX (hp), GTK_WIDGET (d->notebook),
			    TRUE, TRUE, 0);

	/* First page of notebook: Paper selection and margins. */
	gtk_widget_show (l = gtk_label_new (_("Paper")));
	d->priv->config = gnome_print_config_default ();
	u = gnome_print_unit_get_by_name (_("Millimeter"));
	gnome_print_config_set_length (d->priv->config,
		GNOME_PRINT_KEY_PAPER_WIDTH, d->mgr->settings.page.width, u);
	gnome_print_config_set_length (d->priv->config,
		GNOME_PRINT_KEY_PAPER_HEIGHT, d->mgr->settings.page.height, u);
	gnome_print_config_set_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_LEFT,
		d->mgr->settings.margin.left, u);
	gnome_print_config_set_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_RIGHT,
		d->mgr->settings.margin.right, u);
	gnome_print_config_set_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_TOP,
		d->mgr->settings.margin.top, u);
	gnome_print_config_set_length (d->priv->config,
		GNOME_PRINT_KEY_PAGE_MARGIN_BOTTOM,
		d->mgr->settings.margin.bottom, u);
	gnome_print_config_set (d->priv->config,
		GNOME_PRINT_KEY_ORIENTATION,
		d->mgr->settings.orient==GPI_MGR_SETTINGS_ORIENT_90  ? "R90" :
		d->mgr->settings.orient==GPI_MGR_SETTINGS_ORIENT_180 ? "R180":
		d->mgr->settings.orient==GPI_MGR_SETTINGS_ORIENT_270 ? "R270":
		"R0");
	gtk_widget_show (ps = gnome_paper_selector_new_with_flags (
		d->priv->config, GNOME_PAPER_SELECTOR_MARGINS |
			         GNOME_PAPER_SELECTOR_FEED_ORIENTATION));
	gtk_container_set_border_width (GTK_CONTAINER (ps), 5);
	gtk_notebook_append_page (d->notebook, ps, l);

	/* Buttons */
	b = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (b);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (d)->action_area), b,
			  FALSE, FALSE, 0);
	g_signal_connect (b, "clicked", G_CALLBACK (on_cancel_clicked), d);
	b = gtk_button_new_from_stock (GTK_STOCK_PRINT);
	gtk_widget_show (b);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (d)->action_area), b,
			  FALSE, FALSE, 0);
	g_signal_connect (b, "clicked", G_CALLBACK (on_print_clicked), d);
	b = gtk_button_new_from_stock (GTK_STOCK_PRINT_PREVIEW);
	gtk_widget_show (b);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (d)->action_area), b,
			  FALSE, FALSE, 0);
	g_signal_connect (b, "clicked", G_CALLBACK (on_print_preview_clicked),
			  d);
	b = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_widget_show (b);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (d)->action_area), b,
			  FALSE, FALSE, 0);
	gtk_widget_grab_focus (b);
	g_signal_connect (b, "clicked", G_CALLBACK (on_ok_clicked), d);

	/*
	 * Update the settings from gnome-print now and each time the user
	 * switches between pages.
	 */
	gpi_dialog_check_paper (d);
	g_signal_connect (d->notebook, "switch_page",
			  G_CALLBACK (on_switch_page), d);
}
