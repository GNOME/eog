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
#include <gtk/gtkmessagedialog.h>

typedef struct {
	const char *label;
	const char *suffix;
} FileTypeInfo;


struct _EogFileSelectionPrivate {
	GtkWidget            *options;
	FileTypeInfo         *last_info;

	FileTypeInfo         *supported_types;
	gboolean             ensure_suffix;
};

static FileTypeInfo file_types_load[] =  {
	{ N_("By Extension"), "" },
	{ N_("BMP"),  "*.bmp" },
	{ N_("GIF"),  "*.gif" },
	{ N_("ICO"),  "*.ico" },
	{ N_("JPEG"), "*.jpg" },
	{ N_("PNG"),  "*.png" },
	{ N_("PNM"),  "*.pnm" },
	{ N_("RAS"),  "*.ras" },
	{ N_("SVG"),  "*.svg" },
	{ N_("TGA"),  "*.tga" },
	{ N_("TIFF"), "*.tif" },
	{ N_("XBM"),  "*.xbm" },
	{ N_("XPM"),  "*.xpm" },
	{ NULL, NULL }
};

static FileTypeInfo file_types_save[] =  {
	{ N_("By Extension"), "" },
#ifdef HAVE_JPEG
	{ N_("JPEG"), "*.jpg" },
#endif
#ifdef HAVE_PNG
	{ N_("PNG"),  "*.png" },
#endif
#ifdef HAVE_XPM 
	{ N_("XPM"),  "*.xpm" },
#endif
	{ NULL, NULL }
};


#define FILE_TYPE_INFO_KEY  "File Type Info"

GNOME_CLASS_BOILERPLATE (EogFileSelection,
			 eog_file_selection,
			 GtkFileSelection,
			 GTK_TYPE_FILE_SELECTION);

static void
eog_file_selection_dispose (GObject *object)
{
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
eog_append_menu_entry (GtkWidget *menu, FileTypeInfo *info) 
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label (info->label);
	g_object_set_data (G_OBJECT (item), FILE_TYPE_INFO_KEY, info);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static gboolean
is_filename_valid (GtkDialog *dlg)
{
	EogFileSelection *filesel;
	EogFileSelectionPrivate *priv;
	FileTypeInfo *info;
	GtkWidget *menu;
	GtkWidget *item;
	const gchar *filename;
	gint i;

	g_return_val_if_fail (EOG_IS_FILE_SELECTION (dlg), TRUE);

	filesel = EOG_FILE_SELECTION (dlg);
	priv = filesel->priv;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (filesel->priv->options));
	item = gtk_menu_get_active (GTK_MENU (menu));
	g_assert (item != NULL);
	info = g_object_get_data (G_OBJECT (item), FILE_TYPE_INFO_KEY);
	
	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (dlg));

	if (g_strcasecmp (info->suffix, "") == 0) { /* check by extension */
		
		for (i = 0; priv->supported_types[i].label != NULL; i++) {
			if (g_pattern_match_simple (priv->supported_types[i].suffix, filename))
				break;
		}
		
		return (priv->supported_types[i].label != NULL);
	}
	else { /* check specific suffix */
		if (!g_pattern_match_simple (info->suffix, filename)) {
			gchar *tmp;
			tmp = g_strconcat (filename, info->suffix + 1, NULL);
			gtk_file_selection_set_filename (GTK_FILE_SELECTION (dlg), tmp);
		}		
	}

	return TRUE;
}

static void
changed_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *menu;
	GtkWidget *item;
	FileTypeInfo *info;
	const gchar *filename;
	EogFileSelectionPrivate *priv;
	gboolean do_completion = FALSE;

	priv = EOG_FILE_SELECTION (data)->priv;

	/* obtain selected file type info struct */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget));
	item = gtk_menu_get_active (GTK_MENU (menu));

	info = g_object_get_data (G_OBJECT (item), FILE_TYPE_INFO_KEY);

	/* Check if we should perform file completion by suffix.
	 * This is mainly to prevent the deletion of filenames entered by
	 * the user when browsing through the available file types. 
	 * gtk_file_selection_complete is fairly limited wrt this.
	 */
	filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (data));
	if (g_file_test (filename, G_FILE_TEST_IS_DIR))
	{
		do_completion = TRUE;
	}
	else if (priv->last_info != NULL) {
		gchar *basename = g_path_get_basename (filename);

		if (g_strcasecmp (basename, priv->last_info->suffix) == 0) {
			do_completion = TRUE;
		}
		g_free (basename);
	}

	if (do_completion) {
		gtk_file_selection_complete (GTK_FILE_SELECTION (data), info->suffix);		
	}
	
	if (g_strcasecmp (info->suffix, "") == 0) 
		priv->last_info = NULL;
	else 
		priv->last_info = info;
}

static void
response_cb (GtkDialog *dlg, gint id, gpointer data)
{
	if (id == GTK_RESPONSE_OK && !is_filename_valid (dlg)) {
		GtkWidget *dialog;

		g_signal_stop_emission_by_name (G_OBJECT (dlg), "response");

		dialog = gtk_message_dialog_new (GTK_WINDOW (dlg),
						 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE,
						 _("Unsupported file format."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

static void
eog_file_selection_construct (GtkWidget *widget)
{
	EogFileSelection *filesel;
	GtkWidget *hbox;
	GtkWidget *item;
	GtkWidget *menu;
	FileTypeInfo *info;
	gint i;

	filesel = EOG_FILE_SELECTION (widget);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox),
			    gtk_label_new (_("Determine File Type:")),
			    FALSE, FALSE, 0);
	filesel->priv->options = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	
	for (i = 0; filesel->priv->supported_types[i].label != NULL; i++) {
		eog_append_menu_entry (menu, &filesel->priv->supported_types[i]);
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
	gchar *title;
	EogFileSelectionPrivate *priv;

	filesel = GTK_WIDGET (g_object_new (EOG_TYPE_FILE_SELECTION,
					    "show_fileops", TRUE,
					    "select_multiple", FALSE,
					    NULL));

	priv = EOG_FILE_SELECTION (filesel)->priv;

	switch (type) {
	case EOG_FILE_SELECTION_LOAD:
		priv->supported_types = file_types_load;
		priv->ensure_suffix = FALSE;
		title = _("Load Image");
		break;
	case EOG_FILE_SELECTION_SAVE:
		priv->supported_types = file_types_save;
		priv->ensure_suffix = TRUE;
		title = _("Save Image");
		break;
	default:
		g_assert_not_reached ();
	}
	priv->last_info = NULL;
	
	eog_file_selection_construct (filesel);

	gtk_window_set_title (GTK_WINDOW (filesel), title);
	g_signal_connect (G_OBJECT (filesel), "response", G_CALLBACK (response_cb), NULL);

	return filesel;
}

