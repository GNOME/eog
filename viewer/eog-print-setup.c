#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <eog-image-view.h>
#include <eog-image.h>
#include <eog-print-setup.h>
#include <eog-preview.h>
#include <gconf/gconf-client.h>

#define PARENT_TYPE GNOME_TYPE_DIALOG
static GnomeDialogClass *parent_class = NULL;

struct _EogPrintSetupPrivate 
{
	EogImageView	*image_view;
	GConfClient	*client;

	GtkWidget	*adjust_to;
	GtkWidget	*right_down;
	GtkWidget	*down_right;
};

static void
on_right_down_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	if (button->active) {
		gconf_client_set_bool (print_setup->priv->client,
			       "/apps/eog/viewer/down_right", FALSE, NULL);
		gtk_widget_show (print_setup->priv->right_down);
		gtk_widget_hide (print_setup->priv->down_right);
	}
}

static void
on_down_right_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	if (button->active) {
		gconf_client_set_bool (print_setup->priv->client,
				"/apps/eog/viewer/down_right", TRUE, NULL);
		gtk_widget_show (print_setup->priv->down_right);
		gtk_widget_hide (print_setup->priv->right_down);
	}
}

static void
on_paper_size_changed (GtkEntry *entry, EogPrintSetup *print_setup)
{
	gchar	*text;

	text = gtk_entry_get_text (entry);

	gconf_client_set_string (print_setup->priv->client, 
				 "/apps/eog/viewer/paper_size", text, NULL);
}

static void
on_horizontally_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	gconf_client_set_bool (print_setup->priv->client,
			       "/apps/eog/viewer/horizontally", button->active,
			       NULL);
}

static void
on_vertically_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	gconf_client_set_bool (print_setup->priv->client,
			       "/apps/eog/viewer/vertically", button->active,
			       NULL);
}

static void
on_cut_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	gconf_client_set_bool (print_setup->priv->client,
			       "/apps/eog/viewer/cut", button->active, NULL);
}

static void
on_landscape_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	if (button->active)
		gconf_client_set_bool (print_setup->priv->client,
				       "/apps/eog/viewer/landscape", TRUE, 
				       NULL);
}

static void
on_portrait_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	if (button->active)
		gconf_client_set_bool (print_setup->priv->client,
				       "/apps/eog/viewer/landscape", FALSE,
				       NULL);
}

static void
on_adjust_to_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	if (button->active) {
		gconf_client_set_bool (print_setup->priv->client,
				       "/apps/eog/viewer/fit_to_page", FALSE, 
				       NULL);
		gtk_widget_set_sensitive (print_setup->priv->adjust_to, TRUE); 
	}
}

static void
on_fit_to_page_toggled (GtkToggleButton *button, EogPrintSetup *print_setup)
{
	if (button->active) {
		gconf_client_set_bool (print_setup->priv->client,
				       "/apps/eog/viewer/fit_to_page", TRUE,
				       NULL);
		gtk_widget_set_sensitive (print_setup->priv->adjust_to, FALSE);
	}
}

static void
on_top_value_changed (GtkAdjustment *adjustment, EogPrintSetup *print_setup)
{
	gconf_client_set_int (print_setup->priv->client,
			"/apps/eog/viewer/top", adjustment->value, NULL);
}

static void
on_right_value_changed (GtkAdjustment *adjustment, EogPrintSetup *print_setup)
{
	gconf_client_set_int (print_setup->priv->client,
			"/apps/eog/viewer/right", adjustment->value, NULL);
}


static void
on_left_value_changed (GtkAdjustment *adjustment, EogPrintSetup *print_setup)
{
	gconf_client_set_int (print_setup->priv->client,
			"/apps/eog/viewer/left", adjustment->value, NULL);
}

static void
on_bottom_value_changed (GtkAdjustment *adjustment, EogPrintSetup *print_setup)
{
	gconf_client_set_int (print_setup->priv->client,
			"/apps/eog/viewer/bottom", adjustment->value, NULL);
}

static void
on_adjust_to_value_changed (GtkAdjustment *adjustment, 
			    EogPrintSetup *print_setup)
{
	gconf_client_set_int (print_setup->priv->client, 
			"/apps/eog/viewer/adjust_to", adjustment->value, NULL);
}

static void
on_dialog_clicked (GnomeDialog *dialog, gint button_number, 
		   EogPrintSetup *print_setup)
{
	switch (button_number) {
	case 0:
		gtk_widget_unref (GTK_WIDGET (print_setup));
		break;
	case 1:
		eog_image_view_print (print_setup->priv->image_view, FALSE);
		gtk_widget_unref (GTK_WIDGET (print_setup));
		break;
	case 2:
		eog_image_view_print (print_setup->priv->image_view, TRUE);
		break;
	default:
		break;
	}
}

static void
on_pts_activate (GtkMenuItem *item, EogPrintSetup *print_setup)
{
	g_warning ("Not implemented!");
}

static void
on_cm_activate (GtkMenuItem *item, EogPrintSetup *print_setup)
{
	g_warning ("Not implemented!");
}

static void
on_mm_activate (GtkMenuItem *item, EogPrintSetup *print_setup)
{
	g_warning ("Not implemented!");
}

static void
on_In_activate (GtkMenuItem *item, EogPrintSetup *print_setup)
{
	g_warning ("Not implemented!");
}

static void
eog_print_setup_destroy (GtkObject *object)
{
	EogPrintSetup *print_setup;

	print_setup = EOG_PRINT_SETUP (object);

	gtk_object_unref (GTK_OBJECT (print_setup->priv->client));

	g_free (print_setup->priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
eog_print_setup_init (EogPrintSetup *print_setup)
{
	print_setup->priv = g_new0 (EogPrintSetupPrivate, 1);
}

static void
eog_print_setup_class_init (EogPrintSetupClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = eog_print_setup_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
}

GtkType
eog_print_setup_get_type (void)
{
	static GtkType print_setup_type = 0;

	if (!print_setup_type) {
		static const GtkTypeInfo print_setup_info = {
			"EogPrintSetup",
			sizeof (EogPrintSetup),
			sizeof (EogPrintSetupClass),
			(GtkClassInitFunc) eog_print_setup_class_init,
			(GtkObjectInitFunc) eog_print_setup_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		print_setup_type = gtk_type_unique (PARENT_TYPE, 
						    &print_setup_info);
	}

	return (print_setup_type);
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

GtkWidget*
eog_print_setup_new (EogImageView *image_view)
{
	EogPrintSetup 	*new;
	GdkPixbuf	*pixbuf;
	GdkPixmap	*pixmap;
	GdkBitmap	*bitmap;
	GtkWidget	*window;
	GtkWidget	*widget;
	GtkWidget	*notebook;
	GtkWidget	*label;
	GtkWidget	*vbox, *hbox, *table;
	GtkWidget	*button, *combo;
	GtkWidget	*menu, *item;
	GtkObject	*adjustment;
	GSList		*group;
	const gchar	*buttons[] = {GNOME_STOCK_BUTTON_OK,
				      _("Print"),
				      _("Print preview"), NULL};
	gint		 adjust_to, left, bottom, right, top;
	gboolean	 landscape, fit_to_page, horizontally, vertically, cut;
	gboolean	 down_right;
	gchar		*paper_size;

	widget = eog_image_view_get_widget (image_view);
	window = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);

	/* Dialog */
	new = gtk_type_new (EOG_TYPE_PRINT_SETUP);
	gnome_dialog_constructv (GNOME_DIALOG (new), _("Print Setup"), buttons);
	gnome_dialog_set_parent (GNOME_DIALOG (new), GTK_WINDOW (window));
	gnome_dialog_set_close (GNOME_DIALOG (new), FALSE);
	gtk_signal_connect (GTK_OBJECT (new), "clicked", 
			    GTK_SIGNAL_FUNC (on_dialog_clicked), new);
	
	new->priv->client = gconf_client_get_default ();
	new->priv->image_view = image_view;

	landscape = gconf_client_get_bool (new->priv->client, 
					"/apps/eog/viewer/landscape", NULL);
	fit_to_page = gconf_client_get_bool (new->priv->client,
					"/apps/eog/viewer/fit_to_page", NULL);
	horizontally = gconf_client_get_bool (new->priv->client,
					"/apps/eog/viewer/horizontally", NULL);
	vertically = gconf_client_get_bool (new->priv->client,
					"/apps/eog/viewer/vertically", NULL);
	cut = gconf_client_get_bool (new->priv->client,
					"/apps/eog/viewer/cut", NULL);
	adjust_to = gconf_client_get_int (new->priv->client, 
					"/apps/eog/viewer/adjust_to", NULL);
	left = gconf_client_get_int (new->priv->client,
					"/apps/eog/viewer/left", NULL);
	right = gconf_client_get_int (new->priv->client,
					"/apps/eog/viewer/right", NULL);
	bottom = gconf_client_get_int (new->priv->client,
					"/apps/eog/viewer/bottom", NULL);
	top = gconf_client_get_int (new->priv->client,
					"/apps/eog/viewer/top", NULL);
	paper_size = gconf_client_get_string (new->priv->client,
					"/apps/eog/viewer/paper_size", NULL);
	down_right = gconf_client_get_bool (new->priv->client,
					"/apps/eog/viewer/down_right", NULL);
	
	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (new)->vbox), hbox);

	/* Scrolled window for previews */
	window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (window);
	gtk_box_pack_start (GTK_BOX (hbox), window, FALSE, FALSE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
					GTK_POLICY_AUTOMATIC, 
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize (GTK_WIDGET (window), 300, 300);

	/* Preview */
	widget = eog_preview_new (new->priv->client, new->priv->image_view);
	gtk_widget_show (widget);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (window),
					       widget);

	/* Notebook */
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (hbox), notebook, FALSE, FALSE, 0);

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
	gtk_combo_set_popdown_strings (GTK_COMBO (combo), 
				       gnome_paper_name_list ()); 
	gtk_combo_set_value_in_list (GTK_COMBO (combo), TRUE, 0); 
	if (paper_size) {
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), 
				    paper_size); 
		g_free (paper_size);
	}
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO (combo)->entry), "changed", 
			    GTK_SIGNAL_FUNC (on_paper_size_changed), new); 

	/* Orientation */
	make_header (vbox, _("Orientation"));
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	pixbuf = gdk_pixbuf_new_from_file (ICONDIR "/orient-vertical.png");
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 1);
	widget = gtk_pixmap_new (pixmap, bitmap);
	gdk_pixbuf_unref (pixbuf);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (NULL, _("Portrait"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (on_portrait_toggled), new);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	pixbuf = gdk_pixbuf_new_from_file (ICONDIR "/orient-horizontal.png");
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 1);
	widget = gtk_pixmap_new (pixmap, bitmap);
	gdk_pixbuf_unref (pixbuf);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	button = gtk_radio_button_new_with_label (group, _("Landscape"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), landscape);
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (on_landscape_toggled), new);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	/* Margins */
	make_header (vbox, _("Margins"));
	table = gtk_table_new (6, 3, TRUE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	
	/* Units */
	label = gtk_label_new (_("Units"));
//	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 0, 0, 0, 0);
	widget = gtk_option_menu_new ();
//	gtk_widget_show (widget);
	gtk_table_attach (GTK_TABLE (table), widget, 0, 1, 1, 2, 0, 0, 0, 0);
	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
	item = gtk_menu_item_new_with_label (_("pts"));
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate", 
			    GTK_SIGNAL_FUNC (on_pts_activate), new);
	item = gtk_menu_item_new_with_label (_("mm"));
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (on_mm_activate), new);
	item = gtk_menu_item_new_with_label (_("cm"));
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (on_cm_activate), new);
	item = gtk_menu_item_new_with_label (_("In"));
	gtk_widget_show (item);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (on_In_activate), new);

	/* Top margin */
	label = gtk_label_new (_("Top"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, 0, 0, 0, 0);
	adjustment = gtk_adjustment_new (top, 0, 1000, 1, 10, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 1);

	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2, 0, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (on_top_value_changed), new);
	
	/* Left margin */
	label = gtk_label_new (_("Left"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, 0, 0, 0, 0);
	adjustment = gtk_adjustment_new (left, 0, 1000, 1, 10, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 1);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 3, 4, 0, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (on_left_value_changed), new);

	/* Right margin */
	label = gtk_label_new (_("Right"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 2, 3, 2, 3, 0, 0, 0, 0);
	adjustment = gtk_adjustment_new (right, 0, 1000, 1, 10, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 1);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 3, 4, 0, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (on_right_value_changed), new);

	/* Bottom margin */
	label = gtk_label_new (_("Bottom"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 1, 2, 4, 5, 0, 0, 0, 0);
	adjustment = gtk_adjustment_new (bottom, 0, 1000, 1, 10, 10);
	button = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 1);
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 5, 6, 0, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (on_bottom_value_changed), new);

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
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (on_adjust_to_toggled), new);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1, 
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	button = gtk_radio_button_new_with_label (group, _("Fit to page"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), fit_to_page);
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (on_fit_to_page_toggled), new);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2, 
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	adjustment = gtk_adjustment_new (adjust_to, 1, 10000, 1, 10, 10);
	new->priv->adjust_to = gtk_spin_button_new (
					GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_widget_show (new->priv->adjust_to);
	gtk_spin_button_set_update_policy (
			GTK_SPIN_BUTTON (new->priv->adjust_to), 
			GTK_UPDATE_IF_VALID);
	gtk_widget_set_sensitive (new->priv->adjust_to, 
				  !fit_to_page);
	gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
			    GTK_SIGNAL_FUNC (on_adjust_to_value_changed), new);
	gtk_table_attach (GTK_TABLE (table), new->priv->adjust_to, 1, 2, 0, 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
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
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), horizontally);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (on_horizontally_toggled), new);
	button = gtk_check_button_new_with_label (_("Vertically"));
	gtk_widget_show (button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), vertically);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (on_vertically_toggled), new);
	
	/* Third page */
	label = gtk_label_new (_("Printing")); 
	gtk_widget_show (label); 
	vbox = gtk_vbox_new (FALSE, 8); 
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8); 
	gtk_widget_show (vbox); 
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, label); 

	/* Cutting help */
	make_header (vbox, _("Cutting help"));
	button = gtk_check_button_new_with_label (_("Print cutting help"));
	gtk_widget_show (button);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cut);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "toggled", 
			    GTK_SIGNAL_FUNC (on_cut_toggled), new);

	/* Page order */
	make_header (vbox, _("Page order"));
	table = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	/* Down right */
	button = gtk_radio_button_new_with_label (NULL, _("Down, then right"));
	group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	pixbuf = gdk_pixbuf_new_from_file (ICONDIR "/down-right.png");
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 1);
	new->priv->down_right = gtk_pixmap_new (pixmap, bitmap);
	gdk_pixbuf_unref (pixbuf);
	gtk_table_attach (GTK_TABLE (table), new->priv->down_right, 1, 2, 0, 2, 
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	if (down_right) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		gtk_widget_show (new->priv->down_right);
	}
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (on_down_right_toggled), new);
	
	/* Right down */
	button = gtk_radio_button_new_with_label (group, _("Right, then down"));
	gtk_widget_show (button);
	gtk_table_attach (GTK_TABLE (table), button, 0, 1, 1, 2,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	pixbuf = gdk_pixbuf_new_from_file (ICONDIR "/right-down.png");
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 1);
	new->priv->right_down = gtk_pixmap_new (pixmap, bitmap);
	gdk_pixbuf_unref (pixbuf);
	gtk_table_attach (GTK_TABLE (table), new->priv->right_down, 1, 2, 0, 2,
			  GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
	if (!down_right) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		gtk_widget_show (new->priv->right_down);
	}
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			   GTK_SIGNAL_FUNC (on_right_down_toggled), new);
	
	return (GTK_WIDGET (new));
}

