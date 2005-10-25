#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnomeui/libgnomeui.h>

#include "eog-thumbnail.h"

#define THUMB_DEBUG 0
#define EOG_THUMB_ERROR eog_thumb_error_quark ()

static GnomeThumbnailFactory *factory = NULL;

typedef enum {
	EOG_THUMB_ERROR_VFS,
	EOG_THUMB_ERROR_GENERIC,
	EOG_THUMB_ERROR_UNKNOWN
} EogThumbError;


typedef struct {
	char   *uri_str;
	char   *thumb_path;
	time_t  mtime;
	char   *mime_type;
} EogThumbData;

static GQuark
eog_thumb_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("eog-thumb-error-quark");
	
	return q;
}

static void
set_vfs_error (GError **error, GnomeVFSResult result)
{
	g_set_error (error, 
		     EOG_THUMB_ERROR, 
		     EOG_THUMB_ERROR_VFS,
		     gnome_vfs_result_to_string (result));
}

static void
set_thumb_error (GError **error, int error_id, const char *string) 
{
	g_set_error (error, 
		     EOG_THUMB_ERROR, 
		     error_id,
		     string);
}

static GdkPixbuf*
get_valid_thumbnail (EogThumbData *data, GError **error)
{
	char *uri_str = NULL;
	GdkPixbuf *thumb = NULL;

	g_return_val_if_fail (data != NULL, NULL);

	/* does a thumbnail under the path exists? */
	if (g_file_test (data->thumb_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		thumb = gdk_pixbuf_new_from_file (data->thumb_path, error);
		
		/* is this thumbnail file up to date=? */
		if (thumb != NULL && !gnome_thumbnail_is_valid (thumb, data->uri_str, data->mtime)) {
			g_object_unref (thumb);
			thumb = NULL;
#if THUMB_DEBUG
			g_print ("uri: %s, thumbnail is invalid\n", uri_str);
#endif
		}
	}
#if THUMB_DEBUG
	else {
		g_print ("uri: %s, has no thumbnail file\n", uri_str);
	}
#endif
	return thumb;
}

static GdkPixbuf* 
create_thumbnail (EogThumbData *data, GError **error)
{
	GdkPixbuf *thumb = NULL;
	
#if THUMB_DEBUG
	g_print ("create thumbnail for uri: %s\n -> mtime: %i\n -> mime_type; %s\n -> thumbpath: %s\n", 
		 data->uri_str, (int) data->mtime, data->mime_type, data->thumb_path);
#endif
	g_assert (factory != NULL);
	
	if (gnome_thumbnail_factory_can_thumbnail (factory, data->uri_str, data->mime_type, data->mtime)) 
	{
		thumb = gnome_thumbnail_factory_generate_thumbnail (factory, data->uri_str, data->mime_type);
		
		if (thumb != NULL) {
			gnome_thumbnail_factory_save_thumbnail (factory, thumb, data->uri_str, data->mtime);
		}
		else {
			set_thumb_error (error, EOG_THUMB_ERROR_GENERIC, "Thumbnail creation failed");
		}
	}
	else {
		set_thumb_error (error, EOG_THUMB_ERROR_GENERIC, "Thumbnail creation failed");
	}
	
	return thumb;
}

static void
eog_thumb_data_free (EogThumbData *data)
{
	if (data == NULL)
		return;

	if (data->thumb_path != NULL)
		g_free (data->thumb_path);

	if (data->mime_type != NULL)
		g_free (data->mime_type);

	if (data->uri_str != NULL)
		g_free (data->uri_str);

	g_free (data);
}

static EogThumbData*
eog_thumb_data_new (GnomeVFSURI *uri, GError **error)
{
	EogThumbData *data;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);
	
	data = g_new0 (EogThumbData, 1);
	
	data->uri_str    = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE);
	data->thumb_path = gnome_thumbnail_path_for_uri (data->uri_str, GNOME_THUMBNAIL_SIZE_NORMAL);

	info    = gnome_vfs_file_info_new ();
	result  = gnome_vfs_get_file_info_uri (uri, info, 
					       GNOME_VFS_FILE_INFO_DEFAULT |
					       GNOME_VFS_FILE_INFO_FOLLOW_LINKS |
					       GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	
	if (result != GNOME_VFS_OK) {
		set_vfs_error (error, result);
	}
			
	/* check required info fields */
	if (((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) == 0) ||
	    ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) == 0))
	{
		set_thumb_error (error, EOG_THUMB_ERROR_GENERIC, "MTime or mime type not available");
	}
	
	if (*error == NULL) {
		/* if available, copy data */
		data->mtime = info->mtime;
		data->mime_type = g_strdup (info->mime_type);
	}
	else {
		eog_thumb_data_free (data);
		data = NULL;
	}

	gnome_vfs_file_info_unref (info);

	return data;
}


GdkPixbuf*
eog_thumbnail_load (GnomeVFSURI *uri, EogJob *job, GError **error)
{
	GdkPixbuf *thumb = NULL;
	EogThumbData *data;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);
	
	data = eog_thumb_data_new (uri, error);
	if (data == NULL)
		return NULL;

	/* check if there is already a valid thumbnail */
	thumb = get_valid_thumbnail (data, error);

	if (*error == NULL && thumb == NULL) {
		thumb = create_thumbnail (data, error);
	}
	
	eog_thumb_data_free (data);

	return thumb;
}

void
eog_thumbnail_init (void)
{
	if (factory == NULL) {
		factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
	}
}
