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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "eog-hig-dialog.h"
#include "eog-pixbuf-util.h"

struct _EogFileSelectionPrivate {
	EogFileSelectionType type;
	GtkWidget            *options;
	GdkPixbufFormat      *last_info;

	GSList               *supported_types;
	gboolean             allow_directories;
	char                 *path;
};

static char* last_dir[] = { NULL, NULL };

#define FILE_TYPE_INFO_KEY  "File Type Info"

GNOME_CLASS_BOILERPLATE (EogFileSelection,
			 eog_file_selection,
			 GtkFileSelection,
			 GTK_TYPE_FILE_SELECTION);

static void
eog_file_selection_dispose (GObject *object)
{
	EogFileSelectionPrivate *priv;

	priv = EOG_FILE_SELECTION (object)->priv;
	if (priv->supported_types != NULL) {
		g_slist_free (priv->supported_types);
		priv->supported_types = NULL;
	}

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_file_selection_class_init (EogFileSelectionClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->dispose = eog_file_selection_dispose;
}

static void
eog_file_selection_instance_init (EogFileSelection *filesel)
{
	EogFileSelectionPrivate *priv;

	priv = g_new0 (EogFileSelectionPrivate, 1);

	filesel->priv = priv;
}

static void
eog_append_menu_entry (GtkWidget *menu, GdkPixbufFormat *format) 
{
	GtkWidget *item;
	
	if (format == NULL) {
		item = gtk_menu_item_new_with_label (_("By Extension"));
	}
	else {
		char *caption = NULL;
		char **suffix;
		char *name;
		char *tmp;
		int i;

		suffix = gdk_pixbuf_format_get_extensions (format);
		name = gdk_pixbuf_format_get_name (format);
		caption = g_ascii_strup (name, -1);
		for (i = 0; suffix[i] != NULL; i++) {
			if (i == 0) {
				tmp = g_strconcat (caption, " (*.", suffix[i], NULL);
			}
			else {
				tmp = g_strconcat (caption, ", *.", suffix[i], NULL);
			}
			g_free (caption);
			caption = tmp;
		}
		
		tmp = g_strconcat (caption, ")", NULL);
		g_free (caption);
		caption = tmp;

		item = gtk_menu_item_new_with_label (caption);

		g_free (caption);
		g_free (name);
		g_strfreev (suffix);
	}
	g_object_set_data (G_OBJECT (item), FILE_TYPE_INFO_KEY, format);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static gboolean
check_extension (const char *filename, GdkPixbufFormat *format)
{
	int i;
	char **suffixes;
	gboolean result;
	
	suffixes = gdk_pixbuf_format_get_extensions (format);
	if (suffixes == NULL) return TRUE;
	
	for (i = 0; suffixes[i] != NULL; i++) {
		char *pattern = g_strconcat ("*.", suffixes[i], NULL);
		gboolean match;
		
		match = g_pattern_match_simple (pattern, filename);
		g_free (pattern);
		
		if (match) {
			break;
		}
	}

	result = (suffixes [i] != NULL);

	g_strfreev (suffixes);
	
	return result;
}

/* FIXME: the function name doesn't really reflect it's purpose */
static gboolean
is_filename_valid (GtkDialog *dlg)
{
	EogFileSelection *filesel;
	EogFileSelectionPrivate *priv;
	GdkPixbufFormat *info;
	GtkWidget *menu;
	GtkWidget *item;
	const gchar *filename;
	gboolean result = TRUE;;

	g_return_val_if_fail (EOG_IS_FILE_SELECTION (dlg), TRUE);

	filesel = EOG_FILE_SELECTION (dlg);
	priv = filesel->priv;

	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dlg));
	if (priv->allow_directories && g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		return TRUE;
	}

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (filesel->priv->options));
	item = gtk_menu_get_active (GTK_MENU (menu));
	g_assert (item != NULL);
	info = g_object_get_data (G_OBJECT (item), FILE_TYPE_INFO_KEY);

	if (info != NULL) { 
                /* check specific extension */
		if (!check_extension (filename, info)) {
			char *new_filename;
			char **suffix = gdk_pixbuf_format_get_extensions (info);

			new_filename = g_strconcat (filename, ".", suffix[0], NULL);
			gtk_file_selection_set_filename (GTK_FILE_SELECTION (dlg), new_filename);
			g_free (new_filename);
			g_strfreev (suffix);
		}
	}
	else { 
                /* check any possible extensions */
		GSList *it;

		for (it = priv->supported_types; it != NULL; it = it->next) {
			GdkPixbufFormat *format = (GdkPixbufFormat*) it->data;
			if (check_extension (filename, format)) {
				break;
			}
		}

		result = (it != NULL);
	}

	return result;
}

static void
changed_cb (GtkWidget *widget, gpointer data)
{
	/* FIXME: We try to select only those files here, which match
	   the choosen file type suffix. GtkFileSelection can't handle
	   more complex file completions like multiple suffices for a
	   single file type, so this doesn't work properly. That's why
	   we disabled this functionality for now. Maybe there is a
	   more complex workaround for this.
	*/

#if 0
	GtkWidget *menu;
	GtkWidget *item;
	GdkPixbufFormat *info;
	EogFileSelectionPrivate *priv;

	priv = EOG_FILE_SELECTION (data)->priv;

	/* obtain selected file type info struct */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget));
	item = gtk_menu_get_active (GTK_MENU (menu));

	info = g_object_get_data (G_OBJECT (item), FILE_TYPE_INFO_KEY);

	if (info == NULL) {
		gtk_file_selection_complete (GTK_FILE_SELECTION (data), "*");
	}
	else {
		gchar **suffix = gdk_pixbuf_format_get_extensions (info);
	        gchar *pattern = NULL;

		if (suffix != NULL) {
			pattern = g_strconcat ("*.", suffix[0], NULL);
		}
		else {
			pattern = g_strdup ("*");
		}
		
		gtk_file_selection_complete (GTK_FILE_SELECTION (data), pattern);

		g_strfreev (suffix);
		g_free (pattern);
	}
#endif
}


static void
response_cb (GtkDialog *dlg, gint id, gpointer data)
{
	EogFileSelectionPrivate *priv;
	char *dir;
	const char *filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dlg));

	priv = EOG_FILE_SELECTION (dlg)->priv;

	if (id == GTK_RESPONSE_OK) {
		if (last_dir [priv->type] != NULL)
			g_free (last_dir [priv->type]);
		
		dir = g_path_get_dirname (filename);
		last_dir [priv->type] = g_build_filename (dir, ".", NULL);
		g_free (dir);
		
		if (priv->type == EOG_FILE_SELECTION_SAVE && !is_filename_valid (dlg)) {
			GtkWidget *dialog;
			
			g_signal_stop_emission_by_name (G_OBJECT (dlg), "response");
			
			dialog = eog_hig_dialog_new (GTK_STOCK_DIALOG_ERROR, 
						     _("Unsupported image file format for saving."),
						     NULL,
						     TRUE);
			gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
			
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
	}
}

static void
eog_file_selection_construct (GtkWidget *widget)
{
	EogFileSelection *filesel;
	EogFileSelectionPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *menu;
	GSList *it;

	filesel = EOG_FILE_SELECTION (widget);
	priv = filesel->priv;
	
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox),
			    gtk_label_new (_("Determine File Type:")),
			    FALSE, FALSE, 0);
	priv->options = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	
	eog_append_menu_entry (menu, NULL);
	for (it = priv->supported_types; it != NULL; it = it->next) {
		GdkPixbufFormat *format = (GdkPixbufFormat*) it->data;
		eog_append_menu_entry (menu, format);
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (filesel->priv->options), menu);
	g_signal_connect (G_OBJECT (filesel->priv->options), "changed", 
				  G_CALLBACK (changed_cb), filesel);
	gtk_box_pack_start (GTK_BOX (hbox), filesel->priv->options, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (filesel)->vbox), hbox,
				    FALSE, FALSE, 10);	
	gtk_widget_show_all (hbox);

	g_signal_connect (G_OBJECT (filesel), "response", G_CALLBACK (response_cb), NULL);
}

GtkWidget* 
eog_file_selection_new (EogFileSelectionType type)
{
	GtkWidget *filesel;
	gchar *title = NULL;
	EogFileSelectionPrivate *priv;

	filesel = GTK_WIDGET (gtk_widget_new (EOG_TYPE_FILE_SELECTION,
					      "show_fileops", TRUE,
					      "select_multiple", FALSE,
					      NULL));

	priv = EOG_FILE_SELECTION (filesel)->priv;

	priv->type = type;
	switch (type) {
	case EOG_FILE_SELECTION_LOAD:
		priv->supported_types = gdk_pixbuf_get_formats ();
		priv->allow_directories = TRUE;
		title = _("Load Image");
		break;
	case EOG_FILE_SELECTION_SAVE:
		priv->supported_types = eog_pixbuf_get_savable_formats ();
		priv->allow_directories = FALSE;
		title = _("Save Image");
		break;
	default:
		g_assert_not_reached ();
	}

	eog_file_selection_construct (filesel);
	if (last_dir[priv->type] != NULL) {
		gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel), last_dir [priv->type]);
	}
	gtk_window_set_title (GTK_WINDOW (filesel), title);
	g_signal_connect (G_OBJECT (filesel), "response", G_CALLBACK (response_cb), NULL);

	return filesel;
}

