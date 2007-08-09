#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libgnomevfs/gnome-vfs.h>
#include "eog-image-save-info.h"
#include "eog-image-private.h"
#include "eog-pixbuf-util.h"
#include "eog-image.h"

G_DEFINE_TYPE (EogImageSaveInfo, eog_image_save_info, G_TYPE_OBJECT)

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

	G_OBJECT_CLASS (eog_image_save_info_parent_class)->dispose (object);
}

static void
eog_image_save_info_init (EogImageSaveInfo *obj)
{

}

static void 
eog_image_save_info_class_init (EogImageSaveInfoClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->dispose = eog_image_save_info_dispose;
}

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
	return (g_ascii_strcasecmp (scheme, "file") == 0);
}

static char*
get_save_file_type_by_uri (const GnomeVFSURI *uri)
{
	GdkPixbufFormat *format;
	char *type = NULL;

	format = eog_pixbuf_get_format_by_vfs_uri (uri);
	if (format != NULL) {
		type = gdk_pixbuf_format_get_name (format);
	}

	return type;
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
	info->exists       = gnome_vfs_uri_exists (info->uri);
	info->local        = is_local_uri (info->uri);
        info->has_metadata = eog_image_has_data (image, EOG_IMAGE_DATA_EXIF);
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
	info->exists       = gnome_vfs_uri_exists (info->uri);
	info->local        = is_local_uri (info->uri);
        info->has_metadata = FALSE;
	info->modified     = FALSE;
	info->overwrite    = FALSE;

	info->jpeg_quality = -1.0;

	g_assert (info->format != NULL);

	return info;
}
