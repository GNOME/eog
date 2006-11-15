#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomeui/gnome-client.h>
#include <glade/glade.h>
#include "eog-save-as-dialog-helper.h"
#include "eog-pixbuf-util.h"
#include "eog-file-chooser.h"

typedef struct {
	GtkWidget *dir_entry;
	GtkWidget *token_entry;
	GtkWidget *replace_spaces_check;
	GtkWidget *counter_spin;
	GtkWidget *preview_label;
	GtkWidget *format_option;
	GtkWidget *token_option;

	guint      idle_id;
	gint       n_images;
	EogImage  *image;
	gint       nth_image;
} SaveAsData;

static const EogUCInfo uc_info[] = {
        { "String",      "",   FALSE }, /* only used for internal purpose */
        { N_("Filename"),"%f", FALSE },
        { N_("Counter"), "%n", FALSE },
        { N_("Comment"), "%c", TRUE },
        { N_("Date"),    "%d", TRUE },
        { N_("Time"),    "%t", TRUE },
        { N_("Day"),     "%a", TRUE },
        { N_("Month"),   "%m", TRUE },
        { N_("Year"),    "%y", TRUE },
        { N_("Hour"),    "%h", TRUE },
        { N_("Minute"),  "%i", TRUE },
        { N_("Second"),  "%s", TRUE },
        { NULL, NULL, FALSE }
};

static gboolean
update_preview (gpointer user_data)
{
	SaveAsData *data;
	char *preview_str = NULL;
	const char *token_str;
	gboolean convert_spaces;
	gulong   counter_start;
	GdkPixbufFormat *format;
	GtkWidget *item;
	
	data = g_object_get_data (G_OBJECT (user_data), "data");
	g_assert (data != NULL);

	if (data->image == NULL) return FALSE;

	/* obtain required dialog data */
	token_str = gtk_entry_get_text (GTK_ENTRY (data->token_entry));
	convert_spaces = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (data->replace_spaces_check));
	counter_start = gtk_spin_button_get_value_as_int 
		(GTK_SPIN_BUTTON (data->counter_spin));
	item = gtk_menu_get_active 
		(GTK_MENU (gtk_option_menu_get_menu (GTK_OPTION_MENU (data->format_option))));
	g_assert (item != NULL);
	format = g_object_get_data (G_OBJECT (item), "format");

	if (token_str != NULL) {
		/* generate preview filename */
		preview_str = eog_uri_converter_preview (token_str, data->image, format,
							 (counter_start + data->nth_image), 
							 data->n_images,
							 convert_spaces, '_' /* FIXME: make this editable */);
	}

	gtk_label_set_text (GTK_LABEL (data->preview_label), preview_str);

	if (preview_str != NULL)
		g_free (preview_str);

	data->idle_id = 0;

	return FALSE;
}

static void
request_preview_update (GtkWidget *dlg)
{
	SaveAsData *data;
	
	data = g_object_get_data (G_OBJECT (dlg), "data");
	g_assert (data != NULL);

	if (data->idle_id != 0)
		return;

	data->idle_id = g_idle_add (update_preview, dlg);
}

static void
on_browse_button_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *browse_dlg;
	gint response;
	SaveAsData *sd;

	sd = g_object_get_data (G_OBJECT (data), "data");
	g_assert (sd != NULL);
	
	browse_dlg = eog_file_chooser_new (GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	gtk_widget_show_all (browse_dlg);
	response = gtk_dialog_run (GTK_DIALOG (browse_dlg));
	if (response == GTK_RESPONSE_OK) {
		char *folder;

		folder = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (browse_dlg));
		gtk_entry_set_text (GTK_ENTRY (sd->dir_entry), folder);
		gtk_editable_set_position (GTK_EDITABLE (sd->dir_entry), -1);

		g_free (folder);
	}

	gtk_widget_destroy (browse_dlg);
}

static void
on_add_button_clicked (GtkWidget *widget, gpointer user_data)
{
	SaveAsData *data;
	GtkWidget *menu;
	GtkWidget *item;
	int index;
	gboolean has_libexif = FALSE;

	data = g_object_get_data (G_OBJECT (user_data), "data");
	g_assert (data != NULL);

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (data->token_option));
	item = gtk_menu_get_active (GTK_MENU (menu));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "index"));
	
#if HAVE_EXIF
	has_libexif = TRUE;
#endif      

	if (uc_info[index].req_exif && !has_libexif) {
		GtkWidget *dlg;

		dlg = gtk_message_dialog_new (GTK_WINDOW (user_data),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_MESSAGE_WARNING,
					      GTK_BUTTONS_OK,
					      _("Option not available."));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
					      _("To use this function you need the libexif library. Please install"
						" libexif (http://libexif.sf.net) and recompile Eye of GNOME."));
		gtk_dialog_run (GTK_DIALOG (dlg));
		gtk_widget_destroy (dlg);
	}
	else {
		gtk_entry_append_text (GTK_ENTRY (data->token_entry), uc_info[index].rep);
	}

	request_preview_update (GTK_WIDGET (user_data));
}

static void
on_format_option_changed (GtkWidget *widget, gpointer data)
{
	request_preview_update (GTK_WIDGET (data));
}

static void
on_token_entry_changed (GtkWidget *widget, gpointer user_data)
{
	SaveAsData *data;
	gboolean enable_save;

	data = g_object_get_data (G_OBJECT (user_data), "data");
	g_assert (data != NULL);

	request_preview_update (GTK_WIDGET (user_data));

	enable_save = (strlen (gtk_entry_get_text (GTK_ENTRY (data->token_entry))) > 0);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (user_data), GTK_RESPONSE_OK,
					   enable_save);
}

static void
on_replace_spaces_check_clicked (GtkWidget *widget, gpointer data)
{
	request_preview_update (GTK_WIDGET (data));
}

static void
on_counter_spin_changed (GtkWidget *widget, gpointer data)
{
	request_preview_update (GTK_WIDGET (data));
}

static void
prepare_format_options (SaveAsData *data)
{
	GtkWidget *widget;
	GtkWidget *menu;
	GSList *formats;
	GtkWidget *item;
	GSList *it;

	widget = data->format_option;
	menu = gtk_menu_new ();

	formats = eog_pixbuf_get_savable_formats ();
	for (it = formats; it != NULL; it = it->next) {
		GdkPixbufFormat *f;
		char *suffix;

		f = (GdkPixbufFormat*) it->data;

		suffix = eog_pixbuf_get_common_suffix (f);
		item = gtk_menu_item_new_with_label (suffix);
		g_object_set_data (G_OBJECT (item), "format", f);

		g_free (suffix);
		
		gtk_menu_prepend (GTK_MENU (menu), item);
	}

	item = gtk_menu_item_new_with_label (_("as is"));
	gtk_menu_prepend (GTK_MENU (menu), item);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
	gtk_menu_set_active (GTK_MENU (menu), 0); /* 'as is' as default */
}

static void
prepare_token_options (SaveAsData *data)
{
	GtkWidget *widget;
	GtkWidget *menu;
	GtkWidget *item;
	int i;

	widget = data->token_option;
	menu = gtk_menu_new ();
	
	/* uc_info is defined in eog-uri-converter.h */
	for (i = 1; uc_info[i].description != NULL; i++) {
		char *label;

		label = g_strconcat (uc_info[i].description, " (", uc_info[i].rep, ")", NULL);
		item = gtk_menu_item_new_with_label (label);
		g_free (label);

		g_object_set_data (G_OBJECT (item), "index", GINT_TO_POINTER (i));

		gtk_menu_append (GTK_MENU (menu), item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
}

static void
destroy_data_cb (gpointer data)
{
	SaveAsData *sd;

	sd = (SaveAsData*) data;

	if (sd->image != NULL)
		g_object_unref (sd->image);

	if (sd->idle_id != 0)
		g_source_remove (sd->idle_id);

	g_free (sd);
}

static void
set_default_values (GtkWidget *dlg, GnomeVFSURI *base_uri)
{
	SaveAsData *sd;

	sd = (SaveAsData*) g_object_get_data (G_OBJECT (dlg), "data");

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (sd->counter_spin), 0.0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sd->replace_spaces_check),
				      FALSE);
	if (base_uri == NULL) {
		gtk_entry_set_text (GTK_ENTRY (sd->dir_entry), "");
	}
	else {
		char *uri_str;

		uri_str = gnome_vfs_uri_to_string (base_uri, GNOME_VFS_URI_HIDE_NONE);
		gtk_entry_set_text (GTK_ENTRY (sd->dir_entry), uri_str);
		gtk_editable_set_position (GTK_EDITABLE (sd->dir_entry), -1);

		g_free (uri_str);
	}

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dlg), GTK_RESPONSE_OK, FALSE);

	request_preview_update (dlg);
}

GtkWidget*
eog_save_as_dialog_new (GtkWindow *main, GList *images, GnomeVFSURI *base_uri)
{
	char *filepath;
	GladeXML  *xml;
	GtkWidget *dlg;
	SaveAsData *data;
	GtkWidget *label;
	
	filepath = gnome_program_locate_file (NULL,
					      GNOME_FILE_DOMAIN_APP_DATADIR,
					      "eog/glade/eog.glade",
					      FALSE, NULL);

	g_assert (filepath != NULL);

	xml = glade_xml_new (filepath, "Save As Dialog", "eog");
	g_assert (xml != NULL);
	
	dlg = glade_xml_get_widget (xml, "Save As Dialog");
	gtk_window_set_transient_for (GTK_WINDOW (dlg), GTK_WINDOW (main));
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER_ON_PARENT);

	data = g_new0 (SaveAsData, 1);
	/* init widget references */
	data->dir_entry = glade_xml_get_widget (xml, "dir_entry");
	data->token_entry = glade_xml_get_widget (xml, "token_entry");
	data->token_option = glade_xml_get_widget (xml, "token_option");
	data->replace_spaces_check = glade_xml_get_widget (xml, "replace_spaces_check");
	data->counter_spin = glade_xml_get_widget (xml, "counter_spin");
	data->preview_label = glade_xml_get_widget (xml, "preview_label");
	data->format_option = glade_xml_get_widget (xml, "format_option");

	/* init preview information */
	data->idle_id = 0;
	data->n_images = g_list_length (images);
	data->nth_image = (int) ((float) data->n_images * rand() / (float) (RAND_MAX+1.0));
	g_assert (data->nth_image >= 0 && data->nth_image < data->n_images);
	data->image = g_object_ref (G_OBJECT (g_list_nth_data (images, data->nth_image)));
	g_object_set_data_full (G_OBJECT (dlg), "data", data, destroy_data_cb);

	glade_xml_signal_connect_data (xml, "on_browse_button_clicked",
				       (GCallback) on_browse_button_clicked, dlg);

	glade_xml_signal_connect_data (xml, "on_add_button_clicked",
				       (GCallback) on_add_button_clicked, dlg);

	g_signal_connect (G_OBJECT (data->format_option), "changed",
				       (GCallback) on_format_option_changed, dlg);

	glade_xml_signal_connect_data (xml, "on_token_entry_changed",
				       (GCallback) on_token_entry_changed, dlg);

	g_signal_connect (G_OBJECT (data->replace_spaces_check), "toggled",
			  (GCallback) on_replace_spaces_check_clicked, dlg);

	glade_xml_signal_connect_data (xml, "on_counter_spin_changed",
				       (GCallback) on_counter_spin_changed, dlg);

	label = glade_xml_get_widget (xml, "preview_label_from");
	gtk_label_set_text (GTK_LABEL (label), eog_image_get_caption (data->image));

	prepare_format_options (data);
	prepare_token_options (data);
	
	set_default_values (dlg, base_uri);

	return dlg;
}

EogURIConverter* 
eog_save_as_dialog_get_converter (GtkWidget *dlg)
{
	EogURIConverter *conv;

	SaveAsData *data;
	const char *format_str;
	gboolean convert_spaces;
	gulong   counter_start;
	GdkPixbufFormat *format;
	GtkWidget *item;
	GnomeVFSURI *base_uri;
	const char *base_uri_str;

	data = g_object_get_data (G_OBJECT (dlg), "data");
	g_assert (data != NULL);

	/* obtain required dialog data */
	format_str = gtk_entry_get_text (GTK_ENTRY (data->token_entry));

	convert_spaces = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (data->replace_spaces_check));

	counter_start = gtk_spin_button_get_value_as_int 
		(GTK_SPIN_BUTTON (data->counter_spin));

	item = gtk_menu_get_active 
		(GTK_MENU (gtk_option_menu_get_menu (GTK_OPTION_MENU (data->format_option))));
	g_assert (item != NULL);
	format = g_object_get_data (G_OBJECT (item), "format");

	base_uri_str = gtk_entry_get_text (GTK_ENTRY (data->dir_entry));
	base_uri = gnome_vfs_uri_new (base_uri_str);

	/* create converter object */
	conv = eog_uri_converter_new (base_uri, format, format_str);

	/* set other properties */
	g_object_set (G_OBJECT (conv),
		      "convert-spaces", convert_spaces,
		      "space-character", '_',
		      "counter-start", counter_start,
		      "n-images", data->n_images,
		      NULL);

	gnome_vfs_uri_unref (base_uri);

	return conv;
}
