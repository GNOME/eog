#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "eog-info-view-file.h"

#define MODEL_COLUMN_ATTRIBUTE 0
#define MODEL_COLUMN_VALUE     1

typedef enum {
	ATTR_FILE_NAME,
	ATTR_FILE_WIDTH,
	ATTR_FILE_HEIGHT,
	ATTR_FILE_SIZE,
	ATTR_FILE_LAST
} AttributeFile;

typedef struct {
	char *label;
	char *path;
} AttributeInfo;

static AttributeInfo attribute_list[] = {
	{ N_("Filename"), NULL },
	{ N_("Width"),    NULL },
	{ N_("Height"),   NULL },
	{ N_("Filesize"), NULL }
};
 
struct _EogInfoViewFilePrivate
{
	GtkListStore *model;
};

GNOME_CLASS_BOILERPLATE (EogInfoViewFile,
			 eog_info_view_file,
			 EogInfoViewDetail,
			 EOG_TYPE_INFO_VIEW_DETAIL);


static void
eog_info_view_file_finalize (GObject *object)
{
	EogInfoViewFile *instance = EOG_INFO_VIEW_FILE (object);
	
	g_free (instance->priv);
	instance->priv = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_info_view_file_dispose (GObject *object)
{
	EogInfoViewFile *view;
	EogInfoViewFilePrivate *priv;

	view = EOG_INFO_VIEW_FILE (object);
	priv = view->priv;

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_info_view_file_instance_init (EogInfoViewFile *obj)
{
	EogInfoViewFilePrivate *priv;
	GtkTreeView *view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	int i;

	priv = g_new0 (EogInfoViewFilePrivate, 1);

	obj->priv = priv;
	view = GTK_TREE_VIEW (obj);

	priv->model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	/* tag column */
	cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Attribute"),
		                                           cell, 
                                                           "text", MODEL_COLUMN_ATTRIBUTE,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* value column */
	cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Value"),
		                                           cell, 
                                                           "text", MODEL_COLUMN_VALUE,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);

	for (i = 0; i < ATTR_FILE_LAST; i++) {
		GtkTreeIter iter;
		GtkTreePath *path;

		gtk_list_store_append (priv->model, &iter);
		gtk_list_store_set (priv->model, &iter, MODEL_COLUMN_ATTRIBUTE,  attribute_list[i].label, -1);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->model), &iter);
		attribute_list[i].path = gtk_tree_path_to_string (path);
		gtk_tree_path_free (path);
	}
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (priv->model));
}

static void 
eog_info_view_file_class_init (EogInfoViewFileClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_info_view_file_finalize;
	object_class->dispose = eog_info_view_file_dispose;
}


static void
set_row_data (GtkListStore *model, AttributeFile attr, char* value)
{
	GtkTreeIter iter;
	gboolean iter_valid = FALSE;
	char *utf_value = NULL;

	iter_valid = gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (model), &iter, attribute_list [attr].path);

	g_assert (iter_valid);

	if (value != NULL) {
		if (g_utf8_validate (value, -1, NULL)) {
			utf_value = g_strdup (value);
		}
		else {
			utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
		}
	}
		
	gtk_list_store_set (model, &iter, MODEL_COLUMN_VALUE, utf_value, -1);
	
	if (utf_value != NULL) {
		g_free (utf_value);
	}
}

void
eog_info_view_file_show_data (EogInfoViewFile *view, EogImage *image)
{
	char buffer[32];
	int width = -1;
	int height = -1;
	GnomeVFSFileSize bytes;
	char *size_str = NULL;
	char *caption = NULL;
	EogInfoViewFilePrivate *priv;

	g_return_if_fail (EOG_IS_INFO_VIEW_FILE (view));

	priv = view->priv;

	if (image != NULL) {
		caption = eog_image_get_caption (image);
		eog_image_get_size (image, &width, &height);
		bytes = eog_image_get_bytes (image);
		size_str = gnome_vfs_format_file_size_for_display (bytes);
	}

	set_row_data (priv->model, ATTR_FILE_NAME, caption);
	if (width > -1) {
		g_snprintf (buffer, 32, "%i", width);	
	}
	else {
		buffer[0] = '\0';
	}
	set_row_data (priv->model, ATTR_FILE_WIDTH, buffer);

	if (height > -1) {
		g_snprintf (buffer, 32, "%i", height);	
	}
	else {
		buffer[0] = '\0';
	}
	set_row_data (priv->model, ATTR_FILE_HEIGHT, buffer);

	set_row_data (priv->model, ATTR_FILE_SIZE, size_str);

	if (size_str != NULL) 
		g_free (size_str);
}
