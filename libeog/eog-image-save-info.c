#include <string.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs.h>
#include "eog-image-save-info.h"
#include "eog-image-private.h"
#include "eog-pixbuf-util.h"
#include "eog-image.h"

static void
eog_image_save_info_dispose (GObject *object)
{
	EogImageSaveInfo *info = EOG_IMAGE_SAVE_INFO (object);

	if (info->uri != NULL) {
		gnome_vfs_uri_unref (info->uri);
		info->uri = NULL;
	}

	if (info->format != NULL) {
		g_free (info->format);
		info->format = NULL;
	}
}

static void
eog_image_save_info_instance_init (EogImageSaveInfo *obj)
{

}

static void 
eog_image_save_info_class_init (EogImageSaveInfoClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->dispose = eog_image_save_info_dispose;
}


GNOME_CLASS_BOILERPLATE (EogImageSaveInfo,
			 eog_image_save_info,
			 GObject,
			 G_TYPE_OBJECT);

/* is_local_uri: 
 * 
 * Checks if the URI points to a local file system. This tests simply
 * if the URI scheme is 'file'. This function is used to ensure that
 * we can write to the path-part of the URI with non-VFS aware
 * filesystem calls.
 */
static gboolean 
is_local_uri (const GnomeVFSURI* uri)
{
	const char *scheme;

	g_return_val_if_fail (uri != NULL, FALSE);

	scheme = gnome_vfs_uri_get_scheme (uri);
	return (g_strcasecmp (scheme, "file") == 0);
}

static char*
get_save_file_type_by_uri (const GnomeVFSURI *uri)
{
	char *suffix;
	char *short_name;
	char *suffix_start;
	guint len;
	GdkPixbufFormat *format;
	char *type = NULL;

        /* get unescaped string */
	short_name = gnome_vfs_uri_extract_short_name (uri); 

	/* FIXME: does this work for all locales? */
	suffix_start = g_utf8_strrchr (short_name, -1, '.'); 
	
	if (suffix_start == NULL) 
		return NULL;
	
	len = MAX (0, strlen (suffix_start) - 1);
	suffix = g_strndup (suffix_start+1, len);

	format = eog_pixbuf_get_format_by_suffix (suffix);
	if (format != NULL) {
		type = gdk_pixbuf_format_get_name (format);
	}

	g_free (short_name);
	g_free (suffix);

	return type;
}

static gboolean
file_exists (GnomeVFSURI *uri) 
{
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	
	info = gnome_vfs_file_info_new ();

	result = gnome_vfs_get_file_info_uri  (uri, info,
					       GNOME_VFS_FILE_INFO_DEFAULT);
	gnome_vfs_file_info_unref (info);

	return (result == GNOME_VFS_OK);
}


EogImageSaveInfo* 
eog_image_save_info_from_image (gpointer data)
{
	EogImageSaveInfo *info = NULL;
	EogImage *image;
	
	image = EOG_IMAGE (data);

	g_return_val_if_fail (EOG_IS_IMAGE (image), NULL);

	info = g_object_new (EOG_TYPE_IMAGE_SAVE_INFO, NULL);
	
	info->uri          = eog_image_get_uri (image);
	info->format       = g_strdup (image->priv->file_type);
	info->exists       = file_exists (info->uri);
	info->local        = is_local_uri (info->uri);
        info->has_metadata = eog_image_has_metadata (image);
	info->modified     = eog_image_is_modified (image);
	info->overwrite    = FALSE;
	
	info->jpeg_quality = -1.0;

	return info;
}

EogImageSaveInfo* 
eog_image_save_info_from_uri (const char *txt_uri, GdkPixbufFormat *format)
{
	GnomeVFSURI *uri;
	EogImageSaveInfo *info;

	g_return_val_if_fail (txt_uri != NULL, NULL);

	uri = gnome_vfs_uri_new (txt_uri);

	info = eog_image_save_info_from_vfs_uri (uri, format);
	
	gnome_vfs_uri_unref (uri);

	return info;
}

EogImageSaveInfo* 
eog_image_save_info_from_vfs_uri (GnomeVFSURI *uri, GdkPixbufFormat *format)
{
	EogImageSaveInfo *info;

	g_return_val_if_fail (uri != NULL, NULL);

	info = g_object_new (EOG_TYPE_IMAGE_SAVE_INFO, NULL);
	
	info->uri = gnome_vfs_uri_ref (uri);
	if (format == NULL) {
		info->format = get_save_file_type_by_uri (info->uri);
	}
	else {
		info->format = gdk_pixbuf_format_get_name (format);
	}
	info->exists       = file_exists (info->uri);
	info->local        = is_local_uri (info->uri);
        info->has_metadata = FALSE;
	info->modified     = FALSE;
	info->overwrite    = FALSE;

	info->jpeg_quality = -1.0;

	g_assert (info->format != NULL);

	return info;
}
