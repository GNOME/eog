#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-image-view.h>
#include <eog-image.h>
#include <eog-print-setup.h>
#include <eog-preview.h>
#include <eog-util.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-macros.h>
#include <libgnomeprint/gnome-print-paper.h>

GNOME_CLASS_BOILERPLATE (EogPrintSetup,
			 eog_print_setup,
			 GtkDialog,
			 GTK_TYPE_DIALOG)

struct _EogPrintSetupPrivate 
{
	EogImageView	*image_view;
	EogPreview	*preview;

	GConfClient	*client;

	GtkWidget	*spin_adj;

	GtkWidget	*image_right_down;
	GtkWidget	*image_down_right;

	GtkSpinButton	*spin_top;
	GtkSpinButton	*spin_bottom;
	GtkSpinButton	*spin_left;
	GtkSpinButton	*spin_right;

	GtkSpinButton	*spin_overlap_x;
	GtkSpinButton	*spin_overlap_y;

	gchar		*paper_size;

	guint		 unit;

	gboolean	 landscape;
	gdouble		 width;
	gdouble		 height;

	gdouble		 top;
	gdouble		 bottom;
	gdouble		 left;
	gdouble		 right;

	gboolean	 fit_to_page;
	gint		 adjust_to;

	gboolean	 vertically;
	gboolean	 horizontally;

	gboolean 	 cut;
	gboolean	 down_right;

	gdouble		 overlap_x;
	gdouble 	 overlap_y;
	gboolean	 overlap;
};

static struct 
{
	const gchar 	*short_name;
	const gchar 	*full_name;
	const gdouble 	 multiplier;
	const guint	 digits;
	const gfloat	 step_increment;
	const gfloat 	 page_increment;
} unit [] = 
{
	{N_("pts"), N_("points"),     1.0,         0, 1.0, 10.0},
	{N_("mm"),  N_("millimeter"), 25.4 / 72.0, 1, 1.0, 10.0},
	{N_("cm"),  N_("centimeter"), 2.54 / 72.0, 2, 0.1,  1.0},
	{N_("In"),  N_("inch"),       1 / 72.0,    2, 0.1,  1.0}
};

static void
save_settings (EogPrintSetup *ps)
{
	gconf_client_set_string (ps->priv->client,
		"/apps/eog/viewer/paper_size", ps->priv->paper_size, NULL);
	gconf_client_set_float (ps->priv->client, 
		"/apps/eog/viewer/top", ps->priv->top, NULL);
	gconf_client_set_float (ps->priv->client,
		"/apps/eog/viewer/bottom", ps->priv->bottom, NULL);
	gconf_client_set_float (ps->priv->client,
		"/apps/eog/viewer/left", ps->priv->left, NULL);
	gconf_client_set_float (ps->priv->client,
		"/apps/eog/viewer/right", ps->priv->right, NULL);
	gconf_client_set_float (ps->priv->client,
		"/apps/eog/viewer/overlap_x", ps->priv->overlap_x, NULL);
	gconf_client_set_float (ps->priv->client,
		"/apps/eog/viewer/overlap_y", ps->priv->overlap_y, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/landscape", ps->priv->landscape, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/cut", ps->priv->cut, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/horizontally", ps->priv->horizontally, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/vertically", ps->priv->vertically, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/down_right", ps->priv->down_right, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/fit_to_page", ps->priv->fit_to_page, NULL);
	gconf_client_set_bool (ps->priv->client,
		"/apps/eog/viewer/overlap", ps->priv->overlap, NULL);
	gconf_client_set_int (ps->priv->client,
		"/apps/eog/viewer/adjust_to", ps->priv->adjust_to, NULL);
	gconf_client_set_int (ps->priv->client,
		"/apps/eog/viewer/unit", ps->priv->unit, NULL);
	gconf_client_suggest_sync (ps->priv->client, NULL);
}

static void
on_down_right_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	ps->priv->down_right = button->active;

	if (!ps->priv->down_right) {
		gtk_widget_show (ps->priv->image_right_down);
		gtk_widget_hide (ps->priv->image_down_right);
	} else {
		gtk_widget_show (ps->priv->image_down_right);
		gtk_widget_hide (ps->priv->image_right_down);
	}
}

static void
update_preview (EogPrintSetup *ps)
{
	eog_preview_update (ps->priv->preview, ps->priv->width, 
			    ps->priv->height, ps->priv->bottom, ps->priv->top, 
			    ps->priv->right, ps->priv->left, 
			    ps->priv->vertically, ps->priv->horizontally, 
			    ps->priv->cut, ps->priv->fit_to_page, 
			    ps->priv->adjust_to, ps->priv->overlap_x, 
			    ps->priv->overlap_y, ps->priv->overlap);
}

static void 
adjust_margins (EogPrintSetup *ps)
{
	EogPrintSetupPrivate *priv;

	priv = ps->priv;

	/* Upper limits */
	priv->spin_bottom->adjustment->upper = 
				priv->height * unit [priv->unit].multiplier;
	priv->spin_top->adjustment->upper = 
				priv->height * unit [priv->unit].multiplier;
	priv->spin_left->adjustment->upper = 
				priv->width * unit [priv->unit].multiplier;
	priv->spin_right->adjustment->upper = 
				priv->width * unit [priv->unit].multiplier;
	
	/* Margins */
	if (ps->priv->height - ps->priv->top - ps->priv->bottom < 0) {
		gtk_adjustment_set_value (ps->priv->spin_top->adjustment, 0.0);
		gtk_adjustment_set_value (ps->priv->spin_bottom->adjustment, 
					  0.0);
	}
	if (ps->priv->width - ps->priv->right - ps->priv->left < 0) {
		gtk_adjustment_set_value (ps->priv->spin_left->adjustment, 0.0);
		gtk_adjustment_set_value (ps->priv->spin_right->adjustment, 
					  0.0);
	}
}

static void
on_paper_size_changed (GtkEntry *entry, EogPrintSetup *ps)
{
	g_free (ps->priv->paper_size);
	ps->priv->paper_size = g_strdup (gtk_entry_get_text (entry));

	eog_util_paper_size (ps->priv->paper_size, ps->priv->landscape, 
			     &ps->priv->width, &ps->priv->height);

	adjust_margins (ps);
	update_preview (ps);
}

static void
on_horizontally_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	ps->priv->horizontally = button->active;
	update_preview (ps);
}

static void
on_vertically_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	ps->priv->vertically = button->active;
	update_preview (ps);
}

static void
on_cut_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	ps->priv->cut = button->active;
	update_preview (ps);
}

static void
on_overlap_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	ps->priv->overlap = button->active;
	update_preview (ps);
}

static void
on_landscape_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	gdouble tmp;

	ps->priv->landscape = button->active;

	tmp = ps->priv->height;
	ps->priv->height = ps->priv->width;
	ps->priv->width = tmp;

	adjust_margins (ps);
	update_preview (ps);
}

static void
on_fit_to_page_toggled (GtkToggleButton *button, EogPrintSetup *ps)
{
	ps->priv->fit_to_page = button->active;
	gtk_widget_set_sensitive (ps->priv->spin_adj, !button->active);
	update_preview (ps);
}

static void
on_top_value_changed (GtkAdjustment *adj, EogPrintSetup *ps)
{
	ps->priv->top = adj->value / unit [ps->priv->unit].multiplier;
	if (ps->priv->top + ps->priv->bottom > ps->priv->height)
		gtk_adjustment_set_value (ps->priv->spin_bottom->adjustment,
				(ps->priv->height - ps->priv->top) * 
					unit [ps->priv->unit].multiplier);
	update_preview (ps);
}

static void
on_right_value_changed (GtkAdjustment *adj, EogPrintSetup *ps)
{
	ps->priv->right = adj->value / unit [ps->priv->unit].multiplier;
	if (ps->priv->right + ps->priv->left > ps->priv->width)
		gtk_adjustment_set_value (ps->priv->spin_left->adjustment,
				(ps->priv->width - ps->priv->right) * 
					unit [ps->priv->unit].multiplier);
	update_preview (ps);
}


static void
on_left_value_changed (GtkAdjustment *adj, EogPrintSetup *ps)
{
	ps->priv->left = adj->value / unit [ps->priv->unit].multiplier;
	if (ps->priv->right + ps->priv->left > ps->priv->width)
		gtk_adjustment_set_value (ps->priv->spin_right->adjustment,
				(ps->priv->width - ps->priv->left) * 
					unit [ps->priv->unit].multiplier);
	update_preview (ps);
}

static void
on_bottom_value_changed (GtkAdjustment *adj, EogPrintSetup *ps)
{
	ps->priv->bottom = adj->value / unit [ps->priv->unit].multiplier;
	if (ps->priv->top + ps->priv->bottom > ps->priv->height)
		gtk_adjustment_set_value (ps->priv->spin_top->adjustment,
				(ps->priv->height - ps->priv->bottom) * 
					unit [ps->priv->unit].multiplier);
	update_preview (ps);
}

static void
on_adjust_to_value_changed (GtkAdjustment *adjustment, EogPrintSetup *ps)
{
	ps->priv->adjust_to = adjustment->value;
	update_preview (ps);
}

static void
on_overlap_x_value_changed (GtkAdjustment *adj, EogPrintSetup *ps)
{
	ps->priv->overlap_x = adj->value  / unit [ps->priv->unit].multiplier;
	update_preview (ps);
}

static void
on_overlap_y_value_changed (GtkAdjustment *adj, EogPrintSetup *ps)
{
	ps->priv->overlap_y = adj->value  / unit [ps->priv->unit].multiplier;
	update_preview (ps);
}

static void
on_dialog_response (EogPrintSetup *ps, gint response_id, gpointer user_data)
{
	switch (response_id) {
	case GTK_RESPONSE_OK:
		save_settings (ps);
		gtk_widget_unref (GTK_WIDGET (ps));
		break;
	case GTK_RESPONSE_CANCEL:
		gtk_widget_unref (GTK_WIDGET (ps));
		break;
	case 1:
		eog_image_view_print (ps->priv->image_view, FALSE, 
				      ps->priv->paper_size, ps->priv->landscape,
				      ps->priv->bottom, ps->priv->top, 
				      ps->priv->right, ps->priv->left, 
				      ps->priv->vertically, 
				      ps->priv->horizontally, 
				      ps->priv->down_right, ps->priv->cut, 
				      ps->priv->fit_to_page, 
				      ps->priv->adjust_to, 
				      ps->priv->overlap_x,
				      ps->priv->overlap_y,
				      ps->priv->overlap);
		gtk_widget_unref (GTK_WIDGET (ps));
		break;
	case 2:
		eog_image_view_print (ps->priv->image_view, TRUE,
				      ps->priv->paper_size, ps->priv->landscape,
				      ps->priv->bottom, ps->priv->top, 
				      ps->priv->right, ps->priv->left, 
				      ps->priv->vertically, 
				      ps->priv->horizontally, 
				      ps->priv->down_right, ps->priv->cut, 
				      ps->priv->fit_to_page, 
				      ps->priv->adjust_to,
				      ps->priv->overlap_x,
				      ps->priv->overlap_y,
				      ps->priv->overlap);
		break;
	default:
		break;
	}
}

static void
do_conversion (GtkSpinButton *spin, guint old, guint new)
{
	GtkAdjustment   *adj;

	adj = gtk_spin_button_get_adjustment (spin);
	adj->upper = adj->upper / unit [old].multiplier * unit [new].multiplier;
	adj->value = adj->value / unit [old].multiplier * unit [new].multiplier;
	adj->step_increment = unit [new].step_increment;
	adj->page_increment = unit [new].page_increment;
	gtk_spin_button_configure (spin, adj, 1, unit [new].digits);
}

static void
on_unit_activate (GtkMenuItem *item, EogPrintSetup *ps)
{
	guint		 old;

	old = ps->priv->unit;
	ps->priv->unit = GPOINTER_TO_INT (
			g_object_get_data (G_OBJECT (item), "unit"));

	/* Adjust all values */
	do_conversion (ps->priv->spin_top, old, ps->priv->unit);
	do_conversion (ps->priv->spin_bottom, old, ps->priv->unit);
	do_conversion (ps->priv->spin_left, old, ps->priv->unit);
	do_conversion (ps->priv->spin_right, old, ps->priv->unit);
	do_conversion (ps->priv->spin_overlap_x, old, ps->priv->unit);
	do_conversion (ps->priv->spin_overlap_y, old, ps->priv->unit);
}

static void
eog_print_setup_finalize (GObject *object)
{
	EogPrintSetup *print_setup;

	print_setup = EOG_PRINT_SETUP (object);

	g_object_unref (print_setup->priv->client);
	g_free (print_setup->priv->paper_size);

	g_free (print_setup->priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_print_setup_instance_init (EogPrintSetup *print_setup)
{
	print_setup->priv = g_new0 (EogPrintSetupPrivate, 1);
}

static void
eog_print_setup_class_init (EogPrintSetupClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass *) klass;

	gobject_class->finalize = eog_print_setup_finalize;
}

static void
make_header (GtkWidget *vbox, const gchar* header)
{
	GtkWidget *hbox, *widget;

	hbox = gtk_hbox_new (FALSE, 0); 
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 
	if (header) {
		widget = gtk_label_new (header); 
		gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0); 
	}
	widget = gtk_hseparator_new (); 
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0); 
	gtk_widget_show_all (hbox);
}

static GList*
get_paper_name_list (void)
{
	GList *paper_list = NULL;
	GList *name_list = NULL;
	GList *it;
	GnomePrintPaper *paper;

	paper_list = gnome_print_paper_get_list ();
	
	for (it = paper_list; it != NULL; it = it->next)  {
		paper = (GnomePrintPaper*) it->data;
		name_list = g_list_append (name_list, g_strdup (paper->name));
	}

	gnome_print_paper_free_list (paper_list);

	return name_list;
}

GtkWidget*
eog_print_setup_new (EogImageView *image_view)
{
	EogPrintSetup 		*new;
	EogPrintSetupPrivate 	*priv;
	GtkWidget		*window;
	GtkWidget		*widget;
	GtkWidget		*notebook;
	GtkWidget		*label;
	GtkWidget		*vbox, *hbox, *table;
	GtkWidget		*button, *combo;
	GtkWidget		*menu, *item;
	GtkObject		*adj;
	GSList			*group;
	GList			*list;
	gint			 i;

	widget = eog_image_view_get_widget (image_view);
	window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);

	/* Dialog */
	new = g_object_new (EOG_TYPE_PRINT_SETUP, NULL);
	gtk_window_set_title (GTK_WINDOW (new), _("Print Setup"));
	gtk_window_set_transient_for (GTK_WINDOW (new), GTK_WINDOW (window));
	gtk_dialog_add_buttons (GTK_DIALOG (new),
				GTK_STOCK_OK,
				GTK_RESPONSE_OK,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				_("Print"),
				1,
				_("Print preview"),
				2,
				NULL);

	g_signal_connect (new, "response", 
			  G_CALLBACK (on_dialog_response), NULL);
	
	priv = new->priv;
	priv->client = gconf_client_get_default ();
	priv->image_view = image_view;

	eog_util_load_print_settings (priv->client, 
				      &priv->paper_size, &priv->top, 
				      &priv->bottom, &priv->left, &priv->right, 
				      &priv->landscape, &priv->cut, 
				      &priv->horizontally, &priv->vertically, 
				      &priv->down_right, &priv->fit_to_page, 
				      &priv->adjust_to, &priv->unit, 
				      &priv->overlap_x, &priv->overlap_y, 
				      &priv->overlap);

	eog_util_paper_size (priv->paper_size, priv->landscape, &priv->width,
			     &priv->height);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (new), hbox);

	/* Scrolled window for previews */
	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (window);
	gtk_box_pack_start (GTK_BOX (hbox), window, FALSE, FALSE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC, 
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (GTK_WIDGET (window), 300, 300);

	/* Preview */
	widget = eog_preview_new (new->priv->image_view);
	gtk_widget_show (widget);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (window),
					       widget);
	priv->preview = EOG_PREVIEW (widget);
	update_preview (new);

	vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* Units */
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	widget = gtk_option_menu_new (); 
	gtk_widget_show (widget); 
	gtk_box_pack_end (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	menu = gtk_menu_new (); 
	gtk_widget_show (menu); 
	for (i = 0; i < 4; i++) { 
		item = gtk_menu_item_new_with_label (unit [i].short_name); 
		gtk_widget_show (item); 
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item); 
		g_signal_connect (item, "activate", 
				  G_CALLBACK (on_unit_activate), new); 
		g_object_set_data (G_OBJECT (item), "unit", 
				   GINT_TO_POINTER (i)); 
	} 
	gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu); 
	gtk_option_menu_set_history (GTK_OPTION_MENU (widget), new->priv->unit);
	label = gtk_label_new (_("Units: ")); 
	gtk_widget_show (label); 
	gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	/* Notebook */
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);

	/* First page */
	label = gtk_label_new (_("Paper"));
	gtk_widget_show (label);
	vbox = gtk_vbox_new (FALSE, 8);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_widget_show (vbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label);

	/* Paper size */
	make_header (vbox, _("Paper")); 
	hbox = gtk_hbox_new (FALSE, 8); 
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 
	gtk_widget_show (hbox); 
	label = gtk_label_new (_("Paper size:")); 
	gtk_widget_show (label); 
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0); 
	combo = gtk_combo_new (); 
	gtk_widget_show (combo); 
	gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0); 
	list = get_paper_name_list ();
	gtk_combo_set_popdown_strings (GTK_COMBO (combo), list);
	gtk_combo_set_value_in_list (GTK_COMBO (combo), TRUE, 0); 
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), 
			    priv->paper_size);
	g_signal_connect (GTK_COMBO (combo)->entry, "changed", 
			  G_CALLBACK (on_paper_size_changed), new); 
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

	/* Orientation */
	make_header (vbox, _("Orientation"));
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	widget = gtk_image_new_from_file (ICONDIR "/orient-vertical.png");
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (NULL, _("Portrait"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	widget = gtk_image_new_from_file (ICONDIR "/orient-horizontal.png");
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (group, _("Landscape"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
				      new->priv->landscape);
	gtk_widget_show (button);
	g_signal_connect (button, "toggled",
			  G_CALLBACK (on_landscape_toggled), new);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	/* Margins */
	make_header (vbox, _("Margins"));
	table = gtk_table_new (6, 3, TRUE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	/* Top margin */
	label = gtk_label_new (_("Top:"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, 0, 0, 0, 0);
	adj = gtk_adjustment_new (
				priv->top * unit [priv->unit].multiplier,
				0, priv->height * unit [priv->unit].multiplier,
				unit [priv->unit].step_increment,
				unit [priv->unit].page_increment, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 
				      unit [priv->unit].digits);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2, 
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	priv->spin_top = GTK_SPIN_BUTTON (button);
	g_signal_connect (adj, "value_changed",
			  G_CALLBACK (on_top_value_changed), new);
	
	/* Left margin */
	label = gtk_label_new (_("Left:"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, 0, 0, 0, 0);
	adj = gtk_adjustment_new (
				priv->left * unit [priv->unit].multiplier, 
				0, priv->width * unit [priv->unit].multiplier,
				unit [priv->unit].step_increment, 
				unit [priv->unit].page_increment, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 
				      unit [priv->unit].digits);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 3, 4, 
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	priv->spin_left = GTK_SPIN_BUTTON (button);
	g_signal_connect (adj, "value_changed",
			  G_CALLBACK (on_left_value_changed), new);

	/* Right margin */
	label = gtk_label_new (_("Right:"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 2, 3, 2, 3, 0, 0, 0, 0);
	adj = gtk_adjustment_new (
				priv->right * unit [priv->unit].multiplier,
				0, priv->width * unit [priv->unit].multiplier,
				unit [priv->unit].step_increment,
				unit [priv->unit].page_increment, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 
				      unit [new->priv->unit].digits);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 3, 4, 
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	priv->spin_right = GTK_SPIN_BUTTON (button);
	g_signal_connect (adj, "value_changed",
			  G_CALLBACK (on_right_value_changed), new);

	/* Bottom margin */
	label = gtk_label_new (_("Bottom:"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 4, 5, 0, 0, 0, 0);
	adj = gtk_adjustment_new (
				priv->bottom * unit [priv->unit].multiplier,
				0, priv->height * unit [priv->unit].multiplier,
				unit [priv->unit].step_increment,
				unit [priv->unit].page_increment, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 
				      unit [priv->unit].digits);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 5, 6, 
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	priv->spin_bottom = GTK_SPIN_BUTTON (button);
	g_signal_connect (adj, "value_changed",
			  G_CALLBACK (on_bottom_value_changed), new);

	/* Second page */ 
	label = gtk_label_new (_("Image")); 
	gtk_widget_show (label); 
	vbox = gtk_vbox_new (FALSE, 8); 
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8); 
	gtk_widget_show (vbox); 
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label); 

	/* Scale */
	make_header (vbox, _("Scale"));
	table = gtk_table_new (2, 5, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (NULL, _("Adjust to:"));
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1, 
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	button = gtk_radio_button_new_with_label (group, _("Fit to page"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
				      priv->fit_to_page);
	gtk_widget_show (button);
	g_signal_connect (button, "toggled", 
			  G_CALLBACK (on_fit_to_page_toggled), new);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2, 
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	adj = gtk_adjustment_new (priv->adjust_to, 1, 10000, 1, 10, 10);
	priv->spin_adj = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
	gtk_widget_show (priv->spin_adj);
	g_signal_connect (adj, "value_changed", 
			  G_CALLBACK (on_adjust_to_value_changed), new);
	gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (priv->spin_adj), 
					   GTK_UPDATE_IF_VALID);
	gtk_widget_set_sensitive (priv->spin_adj, !priv->fit_to_page);
	gtk_table_attach (GTK_TABLE (table), priv->spin_adj, 1, 2, 0, 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	/* xgettext:no-c-format */
	label = gtk_label_new (_("% of original size"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1, 
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	
	/* Center on page */
	make_header (vbox, _("Center"));
	hbox = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	button = gtk_check_button_new_with_label (_("Horizontally"));
	gtk_widget_show (button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
				      priv->horizontally);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	g_signal_connect (button, "toggled", 
			    G_CALLBACK (on_horizontally_toggled), new);
	button = gtk_check_button_new_with_label (_("Vertically"));
	gtk_widget_show (button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
				      priv->vertically);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	g_signal_connect (button, "toggled", 
			  G_CALLBACK (on_vertically_toggled), new);

	/* Overlap */
	make_header (vbox, _("Overlapping")); 
	table = gtk_table_new (3, 2, FALSE); 
	gtk_widget_show (table); 
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	label = gtk_label_new (_("Overlap horizontally by "));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	label = gtk_label_new (_("Overlap vertically by "));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, 
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	adj = gtk_adjustment_new (
				priv->overlap_x * unit [priv->unit].multiplier,
				0, priv->width * unit [priv->unit].multiplier,
				unit [priv->unit].step_increment,
				unit [priv->unit].page_increment, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 
				      unit [priv->unit].digits);
	gtk_widget_show (button);
	priv->spin_overlap_x = GTK_SPIN_BUTTON (button);
	gtk_spin_button_set_update_policy (priv->spin_overlap_x,
					   GTK_UPDATE_IF_VALID);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 0, 1,
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	g_signal_connect (adj, "value_changed",
			    G_CALLBACK (on_overlap_x_value_changed), new);
	adj = gtk_adjustment_new (
				priv->overlap_y * unit [priv->unit].multiplier,
				0, priv->height * unit [priv->unit].multiplier,
				unit [priv->unit].step_increment,
				unit [priv->unit].page_increment, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 
				      unit [priv->unit].digits);
	gtk_widget_show (button);
	priv->spin_overlap_y = GTK_SPIN_BUTTON (button);
	gtk_spin_button_set_update_policy (priv->spin_overlap_y,
					   GTK_UPDATE_IF_VALID);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2,
			  GTK_FILL | GTK_EXPAND, 0, 0, 0);
	g_signal_connect (adj, "value_changed", 
			    G_CALLBACK (on_overlap_y_value_changed), new);
	
	/* Third page */
	label = gtk_label_new (_("Printing")); 
	gtk_widget_show (label); 
	vbox = gtk_vbox_new (FALSE, 8); 
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8); 
	gtk_widget_show (vbox); 
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label); 

	/* Helpers */
	make_header (vbox, _("Helpers"));
	button = gtk_check_button_new_with_label (_("Print cutting help"));
	gtk_widget_show (button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), priv->cut);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	g_signal_connect (GTK_OBJECT (button), "toggled", 
			    G_CALLBACK (on_cut_toggled), new);
	button = gtk_check_button_new_with_label (_("Print overlap help"));
	gtk_widget_show (button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
				      priv->overlap);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	g_signal_connect (GTK_OBJECT (button), "toggled",
			    G_CALLBACK (on_overlap_toggled), new);

	/* Page order */
	make_header (vbox, _("Page order"));
	table = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	/* Down right */
	button = gtk_radio_button_new_with_label (NULL, _("Down, then right"));
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	new->priv->image_down_right = gtk_image_new_from_file (ICONDIR "/down-right.png");
	gtk_table_attach (GTK_TABLE (table), new->priv->image_down_right, 
			  1, 2, 0, 2, GTK_FILL | GTK_EXPAND, 
			  GTK_FILL | GTK_EXPAND, 0, 0);
	if (priv->down_right) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		gtk_widget_show (new->priv->image_down_right);
	}
	g_signal_connect (button, "toggled",
			  G_CALLBACK (on_down_right_toggled), new);
	
	/* Right down */
	button = gtk_radio_button_new_with_label (group, _("Right, then down"));
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

	new->priv->image_right_down = gtk_image_new_from_file (ICONDIR "/right-down.png");
	gtk_table_attach (GTK_TABLE (table), new->priv->image_right_down, 
			  1, 2, 0, 2, GTK_FILL | GTK_EXPAND, 
			  GTK_FILL | GTK_EXPAND, 0, 0);
	if (!priv->down_right) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		gtk_widget_show (new->priv->image_right_down);
	}
	
	return (GTK_WIDGET (new));
}

