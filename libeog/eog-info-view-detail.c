#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/gnome-macros.h>

#include "eog-info-view-detail.h"


GNOME_CLASS_BOILERPLATE (EogInfoViewDetail,
			 eog_info_view_detail,
			 GtkTreeView,
			 GTK_TYPE_TREE_VIEW);


static void
eog_info_view_detail_finalize (GObject *object)
{
#if 0	
	EogInfoViewDetail *instance = EOG_INFO_VIEW_DETAIL (object);

	g_free (instance->priv);
	instance->priv = NULL;
#endif

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
eog_info_view_detail_dispose (GObject *object)
{
	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
eog_info_view_detail_instance_init (EogInfoViewDetail *obj)
{
#if 0
	EogInfoViewDetailPrivate *priv;

	priv = g_new0 (EogInfoViewDetailPrivate, 1);

	obj->priv = priv;
#endif
}

static void 
eog_info_view_detail_class_init (EogInfoViewDetailClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_info_view_detail_finalize;
	object_class->dispose = eog_info_view_detail_dispose;
}


#if 0

static gboolean
clear_single_row (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gtk_tree_store_set (GTK_TREE_STORE (model), iter, MODEL_COLUMN_VALUE, " ", -1);
	return FALSE;
}

void
_eog_info_view_detail_clear_values (EogInfoViewDetail *view) 
{
	GtkTreeModel *model;
	int i = 0;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	g_assert (model != NULL);

	gtk_tree_model_foreach (model, clear_single_row, NULL);
}


char*
_eog_info_view_detail_set_row_data (EogInfoViewDetail *view, char *path, char *parent, const char *attribute, const char *value)
{
	GtkTreeIter iter;
	char *utf_attribute = NULL;
	char *utf_value = NULL;
	gboolean iter_valid = FALSE;
	GtkTreeModel *model;

	if (!attribute) return NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	g_assert (model != NULL);

	if (path != NULL) {
		iter_valid = gtk_tree_model_get_iter_from_string (model, &iter, path);
	}

	if (!iter_valid) {
		GtkTreePath *tree_path;
		GtkTreeIter parent_iter;
		gboolean parent_valid = FALSE;

		if (parent != NULL) {
			parent_valid = gtk_tree_model_get_iter_from_string (model, &parent_iter, parent);
		}
		gtk_tree_store_append (store, &iter, parent_valid ? &parent_iter : NULL);

		if (path == NULL) {
			tree_path = gtk_tree_model_get_path (model, &iter);
			
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
	gtk_tree_store_set (store, &iter, MODEL_COLUMN_ATTRIBUTE, utf_attribute, -1);
	g_free (utf_attribute);

	if (value != NULL) {
		if (g_utf8_validate (value, -1, NULL)) {
			utf_value = g_strdup (value);
		}
		else {
			utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
		}
		
		gtk_tree_store_set (store, &iter, MODEL_COLUMN_VALUE, utf_value, -1);
		g_free (utf_value);
	}

	return path;
}
#endif
