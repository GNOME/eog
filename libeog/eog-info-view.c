#include <config.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#if HAVE_EXIF
#include <libexif/exif-entry.h>
#include <libexif/exif-data.h>
#endif

#include "eog-info-view.h"

#define MODEL_COLUMN_ATTRIBUTE 0
#define MODEL_COLUMN_VALUE     1

enum {
	SIGNAL_ID_SIZE_PREPARED,
	SIGNAL_ID_INFO_FINISHED,
	SIGNAL_ID_CHANGED,
	SIGNAL_ID_LAST
};


struct _EogInfoViewPrivate
{
	EogImage *image;
	int image_signal_ids [SIGNAL_ID_LAST];

	GtkTreeModel *model;
	GHashTable   *id_path_hash; /* saves the stringified GtkTreeModel path  
				       for a given EXIF entry id */
};

GNOME_CLASS_BOILERPLATE (EogInfoView, eog_info_view,
			 GtkTreeView, GTK_TYPE_TREE_VIEW);


static void
eog_info_view_dispose (GObject *object)
{
	EogInfoView *view;
	EogInfoViewPrivate *priv;

	view = EOG_INFO_VIEW (object);
	priv = view->priv;

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->id_path_hash) {
		g_hash_table_destroy (priv->id_path_hash);
		priv->id_path_hash = NULL;
	}
}

static void
eog_info_view_finalize (GObject *object)
{
	EogInfoView *view;

	view = EOG_INFO_VIEW (object);

	g_free (view->priv);

	view->priv = NULL;
}

static void
eog_info_view_class_init (EogInfoViewClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	object_class->dispose = eog_info_view_dispose;
	object_class->finalize = eog_info_view_finalize;
}

static void
eog_info_view_instance_init (EogInfoView *view)
{
	EogInfoViewPrivate *priv;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	int s;

	priv = g_new0 (EogInfoViewPrivate, 1);

	view->priv = priv;
	priv->image = NULL;
	for (s = 0; s < SIGNAL_ID_LAST; s++) {
		priv->image_signal_ids [s] = 0;
	} 
	priv->model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));
	priv->id_path_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

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
}

static gboolean
clear_single_row (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gtk_list_store_set (GTK_LIST_STORE (model), iter, MODEL_COLUMN_ATTRIBUTE, " ", MODEL_COLUMN_VALUE, " ", -1);
	return FALSE;
}

static void
clear_values (GtkListStore *store) 
{
	gtk_tree_model_foreach (GTK_TREE_MODEL (store), clear_single_row, NULL);
}

static char*
set_row_data (GtkListStore *store, char *path, const char *attribute, const char *value)
{
	GtkTreeIter iter;
	char *utf_attribute = NULL;
	char *utf_value = NULL;
	gboolean iter_valid = FALSE;

	if (!attribute) return NULL;

	if (path != NULL) {
		iter_valid = gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path);
	}

	if (!iter_valid) {
		GtkTreePath *tree_path;

		gtk_list_store_append (store, &iter);

		if (path == NULL) {
			tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
			
			if (tree_path != NULL) {
				path = gtk_tree_path_to_string (tree_path);
				gtk_tree_path_free (tree_path);
			}
		}
	}

	if (g_utf8_validate (attribute, -1, NULL)) {
		utf_attribute = g_strdup (attribute);
	}
	else {
		utf_attribute = g_locale_to_utf8 (attribute, -1, NULL, NULL, NULL);
	}
	gtk_list_store_set (store, &iter, MODEL_COLUMN_ATTRIBUTE, utf_attribute, -1);
	g_free (utf_attribute);

	if (value != NULL) {
		if (g_utf8_validate (value, -1, NULL)) {
			utf_value = g_strdup (value);
		}
		else {
			utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
		}
		
		gtk_list_store_set (store, &iter, MODEL_COLUMN_VALUE, utf_value, -1);
		g_free (utf_value);
	}

	return path;
}

static void
add_image_size_attribute (EogInfoView *view, EogImage *image, GtkListStore *store)
{
	char buffer[32];
	int width, height;
	GnomeVFSFileSize bytes;
	char *size_str;

	g_return_if_fail (EOG_IS_IMAGE (image));
	g_return_if_fail (EOG_IS_INFO_VIEW (view));
	g_return_if_fail (GTK_IS_LIST_STORE (store));

	eog_image_get_size (image, &width, &height);
	bytes = eog_image_get_bytes (image);

	if (width > -1) {
		g_snprintf (buffer, 32, "%i", width);	
	}
	else {
		buffer[0] = '\0';
	}
	set_row_data (store, "1", _("Width"), buffer);


	if (height > -1) {
		g_snprintf (buffer, 32, "%i", height);	
	}
	else {
		buffer[0] = '\0';
	}

	set_row_data (store, "2",_("Height"), buffer);

	size_str = gnome_vfs_format_file_size_for_display (bytes);
	set_row_data (store, "3", _("Filesize"), size_str);

	g_free (size_str);
}

static void
add_image_filename_attribute (EogInfoView *view, EogImage *image, GtkListStore *store)
{
	char *caption;

	g_return_if_fail (EOG_IS_IMAGE (image));
	g_return_if_fail (EOG_IS_INFO_VIEW (view));
	g_return_if_fail (GTK_IS_LIST_STORE (store));

	caption = eog_image_get_caption (image);
	set_row_data (store, "0", _("Filename"), caption);
}


#if HAVE_EXIF
static void 
exif_entry_cb (ExifEntry *entry, gpointer data)
{
	GtkListStore *store;
	EogInfoView *view;
	EogInfoViewPrivate *priv;
	char *path;

	view = EOG_INFO_VIEW (data);
	priv = view->priv;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
	
	path = g_hash_table_lookup (priv->id_path_hash, GINT_TO_POINTER (entry->tag));

	if (path != NULL) {
		set_row_data (store, path, exif_tag_get_name (entry->tag), exif_entry_get_value (entry));	
	}
	else {
		path = set_row_data (store, NULL, 
				     exif_tag_get_name (entry->tag), exif_entry_get_value (entry));	

		g_hash_table_insert (priv->id_path_hash,
				     GINT_TO_POINTER (entry->tag),
				     path);
	}
}

static void
exif_content_cb (ExifContent *content, gpointer data)
{
	exif_content_foreach_entry (content, exif_entry_cb, data);
}

static void
fill_list_exif (EogInfoView *view, ExifData *ed)
{
	exif_data_foreach_content (ed, exif_content_cb, view);
}
#endif

/*  add_image_exif_attribute
 *
 *  This function will only add information additional to filename,
 *  image width and image height. Main purpose is to present
 *  EXIF information.
 */
static void
add_image_exif_attribute (EogInfoView *view, EogImage *image, GtkListStore *store)
{
#if HAVE_EXIF
	ExifData *ed;
#endif

	g_return_if_fail (EOG_IS_IMAGE (image));
	g_return_if_fail (EOG_IS_INFO_VIEW (view));
	g_return_if_fail (GTK_IS_LIST_STORE (store));

	/* add further information to the info view list here, which are not EXIF dependend 
	 * (e.g. file size) 
	 */

#if HAVE_EXIF
	ed = (ExifData*) eog_image_get_exif_information (image);
	if (ed != NULL) {
		fill_list_exif (view, ed);
		exif_data_unref (ed);
	}
#endif
}

/* loading_size_prepared_cb
 *
 * This function is always called, when the image dimension is known. Therefore
 * we add the image size and width to the info view list here.
 */
static void
loading_size_prepared_cb (EogImage *image, gint width, gint height, gpointer data)
{
	GtkListStore *store;
	EogInfoView *view;

	view = EOG_INFO_VIEW (data);

	if (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) == NULL) return;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));

	add_image_size_attribute (view, image, store);
}

/* loading_info_finished_cb
 * 
 * This function is called if EXIF data has been read by EogImage.
 */
static void
loading_info_finished_cb (EogImage *image, gpointer data)
{
	EogInfoView *view;
	GtkListStore *store;

	g_return_if_fail (EOG_IS_IMAGE (image));

	view = EOG_INFO_VIEW (data);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));

	add_image_size_attribute (view, image, store);
	add_image_exif_attribute (view, image, store);
}


/* image_changed_cb
 *
 * This function is called every time the image changed somehow. We
 * will clear everything and rewrite all attributes.
 */
static void
image_changed_cb (EogImage *image, gpointer data)
{
	EogInfoView *view;
	GtkListStore *store;

	g_return_if_fail (EOG_IS_IMAGE (image));

	view = EOG_INFO_VIEW (data);

	/* create new list */
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
	clear_values (store);

	/* add image attributes */
	add_image_filename_attribute (view, image, store);
	add_image_size_attribute (view, image, store);
	add_image_exif_attribute (view, image, store);
}

void
eog_info_view_set_image (EogInfoView *view, EogImage *image)
{
	EogInfoViewPrivate *priv;
	int s;

	g_return_if_fail (EOG_IS_INFO_VIEW (view));

	priv = view->priv;

	if (priv->image == image)
		return;

	if (priv->image != NULL) {
		for (s = 0; s < SIGNAL_ID_LAST; s++) {
			if (priv->image_signal_ids [s] > 0) {
				g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_signal_ids [s]);
				priv->image_signal_ids [s] = 0;
			}
		}

		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (image == NULL) {
		gtk_tree_view_set_model (GTK_TREE_VIEW (view), NULL);
		return;
	}

	clear_values (GTK_LIST_STORE (priv->model));
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), priv->model);

	g_return_if_fail (EOG_IS_IMAGE (image));

	g_object_ref (image);
	priv->image = image;

	/* display at least the filename */
	add_image_filename_attribute (view, image, GTK_LIST_STORE (priv->model));

	/* prepare additional image information callbacks */
	priv->image_signal_ids [SIGNAL_ID_INFO_FINISHED] = 
		g_signal_connect (G_OBJECT (priv->image), "loading_info_finished", 
				  G_CALLBACK (loading_info_finished_cb), view);

	priv->image_signal_ids [SIGNAL_ID_SIZE_PREPARED] = 
		g_signal_connect (G_OBJECT (priv->image), "loading_size_prepared", 
				  G_CALLBACK (loading_size_prepared_cb), view);

	priv->image_signal_ids [SIGNAL_ID_CHANGED] = 
		g_signal_connect (G_OBJECT (priv->image), "image_changed", 
				  G_CALLBACK (image_changed_cb), view);

	/* start loading */
	eog_image_load (priv->image, EOG_IMAGE_LOAD_DEFAULT);
}


