#include <config.h>
#include "eog-file-selection.h"
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <glib/gslist.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmessagedialog.h>
#include "eog-hig-dialog.h"
#include "eog-pixbuf-util.h"

static char* last_dir[] = { NULL, NULL };

#define FILE_FORMAT_KEY "file-format"

GNOME_CLASS_BOILERPLATE (EogFileSelection,
			 eog_file_selection,
			 GtkFileChooserDialog,
			 GTK_TYPE_FILE_CHOOSER_DIALOG);

static void
eog_file_selection_class_init (EogFileSelectionClass *klass)
{
}

static void
eog_file_selection_instance_init (EogFileSelection *filesel)
{
}

static gboolean
replace_file (GtkWindow *window, const gchar *file_name)
{
       GtkWidget *msgbox;
       gchar *utf_file_name;
       gchar *msg;
       gint ret;

       utf_file_name = g_filename_to_utf8 (file_name, -1,
                                            NULL, NULL, NULL);
       msg = g_strdup_printf (_("Do you want to overwrite %s?"),
			      utf_file_name);
       g_free (utf_file_name);
       
       msgbox = eog_hig_dialog_new (GTK_STOCK_DIALOG_WARNING,
                                    _("File exists"), 
                                    msg,
                                    TRUE);

       gtk_dialog_add_buttons (GTK_DIALOG (msgbox),
                               GTK_STOCK_NO,
                               GTK_RESPONSE_CANCEL,
                               GTK_STOCK_YES,
                               GTK_RESPONSE_YES,
                               NULL);

       gtk_dialog_set_default_response (GTK_DIALOG (msgbox),
                                        GTK_RESPONSE_CANCEL);

       ret = gtk_dialog_run (GTK_DIALOG (msgbox));
       gtk_widget_destroy (msgbox);
       g_free (msg);

       return (ret == GTK_RESPONSE_YES);
}

static void
response_cb (GtkDialog *dlg, gint id, gpointer data)
{
	char *dir;
	GtkFileChooserAction action;
		
	dir = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg));
	action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dlg));
	
        if (action == GTK_FILE_CHOOSER_ACTION_SAVE){
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));

		if (g_file_test (filename, G_FILE_TEST_EXISTS) &&
		    !replace_file (GTK_WINDOW (dlg), filename)) 
		{
			g_signal_stop_emission_by_name (G_OBJECT (dlg), "response");
			g_free (filename);
			return;
		}
		
		g_free (filename);
	}
	
	if (id == GTK_RESPONSE_OK) {
		if (last_dir [action] != NULL)
			g_free (last_dir [action]);
		
		last_dir [action] = dir;
	}
}

static void
eog_file_selection_add_filter (GtkWidget *widget)
{
	EogFileSelection *filesel;
	GSList *it;
	GSList *formats;
	GtkFileFilter *filter;
	GtkFileFilter *all_img_filter;
 	GtkFileFilter *all_file_filter;
	gchar **mime_types, **pattern, *tmp;
	int i;

	filesel = EOG_FILE_SELECTION (widget);

	/* All Files Filter */
	all_file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_file_filter, _("All Files"));
	gtk_file_filter_add_pattern (all_file_filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filesel), all_file_filter);

	/* All Image Filter */
	all_img_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_img_filter, _("All Images"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filesel), all_img_filter);

	if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (filesel)) == GTK_FILE_CHOOSER_ACTION_SAVE) {
		formats = eog_pixbuf_get_savable_formats ();
	}
	else {
		formats = gdk_pixbuf_get_formats ();
	}

	for (it = formats; it != NULL; it = it->next) {
		char *filter_name;
		GdkPixbufFormat *format;
		filter = gtk_file_filter_new ();

		format = (GdkPixbufFormat*) it->data;

		/* Filter name: First description then file extension, eg. "The PNG-Format (*.png)".*/
		filter_name = g_strdup_printf (_("%s (*.%s)"), 
					       gdk_pixbuf_format_get_description (format),
					       gdk_pixbuf_format_get_name (format));
		gtk_file_filter_set_name (filter, filter_name);
		g_free (filter_name);
		
		mime_types = gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) it->data);
		for (i = 0; mime_types[i] != NULL; i++) {
			gtk_file_filter_add_mime_type (filter, mime_types[i]);
			gtk_file_filter_add_mime_type (all_img_filter, mime_types[i]);
		}
		g_strfreev (mime_types);
 
		pattern = gdk_pixbuf_format_get_extensions ((GdkPixbufFormat *) it->data);
		for (i = 0; pattern[i] != NULL; i++) {
			tmp = g_strconcat ("*.", pattern[i], NULL);
			gtk_file_filter_add_pattern (filter, tmp);
			gtk_file_filter_add_pattern (all_img_filter, tmp);
			g_free (tmp);
		}
		g_strfreev (pattern);

		/* attach GdkPixbufFormat to filter, see also
		 * eog_file_selection_get_format. */
		g_object_set_data (G_OBJECT (filter), 
				   FILE_FORMAT_KEY,
				   format);

		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filesel), filter);
		
	}
	g_slist_free (formats);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (filesel), all_img_filter);
}

GtkWidget* 
eog_file_selection_new (GtkFileChooserAction action)
{
	GtkWidget *filesel;
	gchar *title = NULL;

	filesel = g_object_new (EOG_TYPE_FILE_SELECTION,
				"action", action,
				"select-multiple", FALSE /* (action == GTK_FILE_CHOOSER_ACTION_OPEN) */,
				"folder-mode", FALSE,
				NULL);
	switch (action) {
	case GTK_FILE_CHOOSER_ACTION_OPEN:
		gtk_dialog_add_buttons (GTK_DIALOG (filesel),
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					NULL);
		title = _("Load Image");
		break;

	case GTK_FILE_CHOOSER_ACTION_SAVE:
		gtk_dialog_add_buttons (GTK_DIALOG (filesel),
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					NULL);
		title = _("Save Image");
		break;

	default:
		g_assert_not_reached ();
	}

	eog_file_selection_add_filter (filesel);
	if (last_dir[action] != NULL) {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filesel), last_dir [action]);
	}

	g_signal_connect (G_OBJECT (filesel), "response", G_CALLBACK (response_cb), NULL);
 	gtk_window_set_title (GTK_WINDOW (filesel), title);
	gtk_dialog_set_default_response (GTK_DIALOG (filesel), GTK_RESPONSE_OK);
	gtk_window_set_default_size (GTK_WINDOW (filesel), 600, 400);

	return filesel;
}

GtkWidget* 
eog_folder_selection_new (void)
{
	GtkWidget *filesel;

	filesel = g_object_new (EOG_TYPE_FILE_SELECTION,
				"action", GTK_FILE_CHOOSER_ACTION_OPEN,
				"select-multiple", FALSE,
				"folder-mode", TRUE,
				NULL);
	gtk_dialog_add_buttons (GTK_DIALOG (filesel),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_OK,
				NULL);

	if (last_dir[GTK_FILE_CHOOSER_ACTION_OPEN] != NULL) {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filesel), 
						     last_dir [GTK_FILE_CHOOSER_ACTION_OPEN]);
	}

	g_signal_connect (G_OBJECT (filesel), "response", G_CALLBACK (response_cb), NULL);

	gtk_window_set_title (GTK_WINDOW (filesel), _("Open Folder"));
	gtk_dialog_set_default_response (GTK_DIALOG (filesel), GTK_RESPONSE_OK);
	gtk_window_set_default_size (GTK_WINDOW (filesel), 600, 400);

	return filesel;
}

GdkPixbufFormat* 
eog_file_selection_get_format (EogFileSelection *sel)
{
	GtkFileFilter *filter;
	GdkPixbufFormat* format;

	g_return_val_if_fail (EOG_IS_FILE_SELECTION (sel), NULL);

	filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (sel));
	if (filter == NULL) return NULL;
     
	format = g_object_get_data (G_OBJECT (filter), FILE_FORMAT_KEY);
	
	return format;
}
