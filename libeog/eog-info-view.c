#include <config.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#if HAVE_EXIF
#include <libexif/exif-entry.h>
#include <libexif/exif-data.h>
#endif

#include "eog-info-view.h"

struct _EogInfoViewPrivate
{
	EogImage *image;
	int image_cb_id;
	int image_cb_id2;
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

	priv = g_new0 (EogInfoViewPrivate, 1);

	view->priv = priv;
	priv->image = NULL;
	priv->image_cb_id = 0;
	priv->image_cb_id2 = 0;

	/* tag column */
	cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Attribute"),
		                                           cell, 
                                                           "text", 0,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* value column */
	cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Value"),
		                                           cell, 
                                                           "text", 1,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);
}

static void
append_row (GtkListStore *store, const char *attribute, const char *value)
{
	GtkTreeIter iter;
	char *utf_attribute = NULL;
	char *utf_value = NULL;

	if (!attribute) return;

	gtk_list_store_append (store, &iter);

	if (g_utf8_validate (attribute, -1, NULL)) {
		utf_attribute = g_strdup (attribute);
	}
	else {
		utf_attribute = g_locale_to_utf8 (attribute, -1, NULL, NULL, NULL);
	}
	gtk_list_store_set (store, &iter, 0, utf_attribute, -1);
	g_free (utf_attribute);

	if (value != NULL) {
		if (g_utf8_validate (value, -1, NULL)) {
			utf_value = g_strdup (value);
		}
		else {
			utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
		}
		
		gtk_list_store_set (store, &iter, 1, utf_value, -1);
		g_free (utf_value);
	}
}

#if HAVE_EXIF
static void 
exif_entry_cb (ExifEntry *entry, gpointer data)
{
	GtkListStore *store;

	store = GTK_LIST_STORE (data);

	append_row (store, exif_tag_get_name (entry->tag), exif_entry_get_value (entry));	
}

static void
exif_content_cb (ExifContent *content, gpointer data)
{
	exif_content_foreach_entry (content, exif_entry_cb, data);
}

static void
fill_list_exif (EogImage *image, GtkListStore *store, ExifData *ed)
{
	exif_data_foreach_content (ed, exif_content_cb, store);
}
#endif

static void
fill_list (EogInfoView *view)
{
	GtkListStore *store;
	EogImage *image;
	char buffer[32];
	int width, height;
#if HAVE_EXIF
	ExifData *ed;
#endif
	if (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) == NULL) return;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
	image = view->priv->image;

	g_assert (image != NULL);

	eog_image_get_size (image, &width, &height);
	if (width > -1) {
		g_snprintf (buffer, 32, "%i", width);	
		append_row (store, _("Width"), buffer);
	}
	else {
		append_row (store, _("Width"), NULL);
	}

	if (height > -1) {
		g_snprintf (buffer, 32, "%i", height);
		append_row (store, _("Height"), buffer);
	}
	else {
		append_row (store, _("Height"), NULL);
	}

#if HAVE_EXIF
	ed = (ExifData*) eog_image_get_exif_information (image);
	if (ed != NULL) {
		fill_list_exif (image, store, ed);
		exif_data_unref (ed);
	}
#endif
}

static void
loading_size_prepared_cb (EogImage *image, gint width, gint height, gpointer data)
{
	GtkListStore *store;
	char buffer[32];
	GtkTreeIter iter;
	
	EogInfoView *view = EOG_INFO_VIEW (data);

	if (gtk_tree_view_get_model (GTK_TREE_VIEW (view)) == NULL) return;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));

	g_snprintf (buffer, 32, "%i", width);	
	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, "1")) {
		append_row (store, _("Width"), buffer);
	}
	else {
		gtk_list_store_set (store, &iter, 1, buffer, -1);
	}

	g_snprintf (buffer, 32, "%i", height);	
	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, "2")) {
		append_row (store, _("Height"), buffer);
	}
	else {
		gtk_list_store_set (store, &iter, 1, buffer, -1);
	}
}

static void
loading_info_finished_cb (EogImage *image, gpointer data)
{
	fill_list (EOG_INFO_VIEW (data));
}

void
eog_info_view_set_image (EogInfoView *view, EogImage *image)
{
	EogInfoViewPrivate *priv;
	GtkListStore *store;
	char *caption;

	g_return_if_fail (EOG_IS_INFO_VIEW (view));

	priv = view->priv;

	if (priv->image == image)
		return;

	if (priv->image != NULL) {
		if (priv->image_cb_id > 0) {
			g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_cb_id);
			priv->image_cb_id = 0;
		}
		if (priv->image_cb_id2 > 0) {
			g_signal_handler_disconnect (G_OBJECT (priv->image), priv->image_cb_id2);
			priv->image_cb_id2 = 0;
		}

		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (image == NULL) {
		gtk_tree_view_set_model (GTK_TREE_VIEW (view), NULL);
		return;
	}

	g_return_if_fail (EOG_IS_IMAGE (image));

	g_object_ref (image);
	priv->image = image;

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	caption = eog_image_get_caption (image);
	append_row (store, _("Filename"), caption);
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), GTK_TREE_MODEL (store));

	priv->image_cb_id = g_signal_connect (G_OBJECT (priv->image), "loading_info_finished", 
					      G_CALLBACK (loading_info_finished_cb), view);
	priv->image_cb_id2 = g_signal_connect (G_OBJECT (priv->image), "loading_size_prepared", 
					      G_CALLBACK (loading_size_prepared_cb), view);

	eog_image_load (priv->image, EOG_IMAGE_LOAD_DEFAULT);
}


