#include <config.h>
#include <stdlib.h>
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
#include <gtk/gtkimage.h>
#include <gtk/gtkvbox.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomeui/gnome-thumbnail.h>
#include "eog-hig-dialog.h"
#include "eog-pixbuf-util.h"

static char* last_dir[] = { NULL, NULL };

#define FILE_FORMAT_KEY "file-format"


struct _EogFileSelectionPrivate {
	GnomeThumbnailFactory *thumb_factory;

	GtkWidget *image;
	GtkWidget *size_label;
	GtkWidget *dim_label;
	GtkWidget *creator_label;
};

GNOME_CLASS_BOILERPLATE (EogFileSelection,
			 eog_file_selection,
			 GtkFileChooserDialog,
			 GTK_TYPE_FILE_CHOOSER_DIALOG);

static void
eog_file_selection_dispose (GObject *object)
{
	EogFileSelectionPrivate *priv;

	priv = EOG_FILE_SELECTION (object)->priv;

	if (priv->thumb_factory != NULL) {
		g_object_unref (priv->thumb_factory);
		priv->thumb_factory = NULL;
	}

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}


static void
eog_file_selection_finalize (GObject *object)
{
	EogFileSelectionPrivate *priv;

	priv = EOG_FILE_SELECTION (object)->priv;

	g_free (priv);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_file_selection_class_init (EogFileSelectionClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->dispose = eog_file_selection_dispose;
	object_class->finalize = eog_file_selection_finalize;
}

static void
eog_file_selection_instance_init (EogFileSelection *filesel)
{
	EogFileSelectionPrivate *priv;

	priv = g_new0 (EogFileSelectionPrivate, 1);
	filesel->priv = priv;
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

static void
set_preview_label (GtkWidget *label, const char *str)
{
	if (str == NULL) {
		gtk_widget_hide (GTK_WIDGET (label));
	}
	else {
		gtk_label_set_text (GTK_LABEL (label), str);
		gtk_widget_show (GTK_WIDGET (label));
	}
}

/* Sets the pixbuf as preview thumbnail and tries to read and display
 * further information according to the thumbnail spec.
 */
static void
set_preview_pixbuf (EogFileSelection *filesel, GdkPixbuf *pixbuf, GnomeVFSFileInfo *info)
{
	EogFileSelectionPrivate *priv;
	int bytes;
	const char *bytes_str;
	const char *width;
	const char *height;
	const char *creator = NULL; 
	char *size_str    = NULL;
	char *dim_str     = NULL;
		
	g_return_if_fail (EOG_IS_FILE_SELECTION (filesel));

	priv = filesel->priv;
			   
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), pixbuf);
	
	if (pixbuf != NULL) {
		/* try to read file size */
		bytes_str = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Size");
		if (bytes_str != NULL) {
			bytes = atoi (bytes_str);
			size_str = gnome_vfs_format_file_size_for_display (bytes);
		}
		else {
			size_str = gnome_vfs_format_file_size_for_display (info->size);
		}

		/* try to read image dimensions */
		width  = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Image::Width");
		height = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Image::Height");
		
		if ((width != NULL) && (height != NULL)) {
			/* Pixel size of image: width x height in pixel */
			dim_str = g_strdup_printf (_("%s x %s pixel"), width, height);
		}

#if 0
		/* Not sure, if this is really useful, therefore its commented out for now. */

		/* try to read creator of the thumbnail */
		creator = gdk_pixbuf_get_option (pixbuf, "tEXt::Software");

		/* stupid workaround to display nicer string if the
		 * thumbnail is created through the gnome libraries.
		 */
		if (g_ascii_strcasecmp (creator, "Gnome::ThumbnailFactory") == 0) {
			creator = "GNOME Libs";
		}
#endif
	}
	
	set_preview_label (priv->size_label, size_str);
	set_preview_label (priv->dim_label, dim_str);
	set_preview_label (priv->creator_label, creator);

	if (size_str != NULL) {
		g_free (size_str);
	}

	if (dim_str != NULL) {
		g_free (dim_str);
	}
}

static void
update_preview_cb (GtkFileChooser *file_chooser, gpointer data)
{
	EogFileSelectionPrivate *priv;
	char *uri;
	char *thumb_path = NULL;
	GdkPixbuf *pixbuf = NULL;
	gboolean have_preview = FALSE;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	priv = EOG_FILE_SELECTION (file_chooser)->priv;

	uri = gtk_file_chooser_get_preview_uri (file_chooser);
	if (uri == NULL) {
		gtk_file_chooser_set_preview_widget_active (file_chooser, FALSE);
		return;
	}

	info = gnome_vfs_file_info_new ();

	result = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);
	if ((result == GNOME_VFS_OK) && (priv->thumb_factory != NULL)) {
		thumb_path = gnome_thumbnail_factory_lookup (priv->thumb_factory,
							     uri,
							     info->mtime);
		if (thumb_path == NULL) {
			/* read files smaller than 100kb directly */
			if (info->size <= 100000) {
				/* FIXME: we should then output also the image dimensions */
				thumb_path = gtk_file_chooser_get_preview_filename (file_chooser);
			}
		}
				
		if (thumb_path != NULL && g_file_test (thumb_path, G_FILE_TEST_EXISTS)) {
			/* try to load and display preview thumbnail */
			pixbuf = gdk_pixbuf_new_from_file (thumb_path, NULL);
			
			have_preview = (pixbuf != NULL);
		
			set_preview_pixbuf (EOG_FILE_SELECTION (file_chooser), pixbuf, info);
			
			if (pixbuf != NULL) {
				gdk_pixbuf_unref (pixbuf);
			}
		}
	}
	
	if (thumb_path != NULL) {
		g_free (thumb_path);
	}

	g_free (uri);
	gnome_vfs_file_info_unref (info);

	gtk_file_chooser_set_preview_widget_active (file_chooser, have_preview);
}

static void
eog_file_selection_add_preview (GtkWidget *widget)
{
	EogFileSelectionPrivate *priv;
	GtkWidget *vbox;

	priv = EOG_FILE_SELECTION (widget)->priv;
	
	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	priv->image      = gtk_image_new ();
	/* 128x128 is maximum size of thumbnails */
	gtk_widget_set_usize (priv->image, 128,128);
	
	priv->dim_label  = gtk_label_new (NULL);
	priv->size_label = gtk_label_new (NULL);
	priv->creator_label = gtk_label_new (NULL);
	
	gtk_box_pack_start (GTK_BOX (vbox), priv->image, 
			    FALSE, /* expand */
			    TRUE,  /* fill */
			    0      /* padding */
		);
	gtk_box_pack_start (GTK_BOX (vbox), priv->dim_label, 
			    FALSE, /* expand */
			    TRUE,  /* fill */
			    0      /* padding */
		);
	gtk_box_pack_start (GTK_BOX (vbox), priv->size_label, 
			    FALSE, /* expand */
			    TRUE,  /* fill */
			    0      /* padding */
		);
	gtk_box_pack_start (GTK_BOX (vbox), priv->creator_label, 
			    FALSE, /* expand */
			    TRUE,  /* fill */
			    0      /* padding */
		);

	gtk_widget_show_all (vbox);

	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (widget), vbox);
	gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER (widget), FALSE);

	priv->thumb_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);

	g_signal_connect (widget, "update-preview",
			  G_CALLBACK (update_preview_cb), NULL);
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
	eog_file_selection_add_preview (filesel);

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

