#include "cimage.h"
#include <gnome.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>

struct _CImagePrivate {
	guint unique_id;

	GnomeVFSURI *uri;
	
	GdkPixbuf *thumbnail;

	gchar *caption;	
	guint  width;
	guint  height;
	
	gboolean  loading_failed;
	gboolean  is_selected;	
};

static GtkObjectClass *parent_class;

static void cimage_class_init (CImageClass *klass);
static void cimage_init (CImage *image);
static void cimage_finalize (GtkObject *obj);
static void cimage_destroy (GtkObject *obj);


static guint
get_unique_id (void)
{
	static guint last_id = 0;

	return last_id++;
}


GtkType
cimage_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"CollectionImage",
			sizeof (CImage),
			sizeof (CImageClass),
			(GtkClassInitFunc)  cimage_class_init,
			(GtkObjectInitFunc) cimage_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (
			gtk_object_get_type (), &info);
	}

	return type;
}


void
cimage_destroy (GtkObject *obj)
{
	CImagePrivate *priv;
	
	g_return_if_fail (obj != NULL);
	g_return_if_fail (IS_CIMAGE (obj));

	priv = CIMAGE (obj)->priv;

	if (priv->uri) {
		gnome_vfs_uri_unref (priv->uri);
		priv->uri = NULL;
	}

	if (priv->thumbnail) {
		gdk_pixbuf_unref (priv->thumbnail);
		priv->thumbnail = NULL;
	}
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

void
cimage_finalize (GtkObject *obj)
{
	CImage *img;

	img = CIMAGE (obj);

	if (img->priv)
		g_free (img->priv);

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		GTK_OBJECT_CLASS (parent_class)->finalize (obj);
}

void 
cimage_class_init (CImageClass *klass)
{
	GtkObjectClass *obj_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	obj_class->destroy = cimage_destroy;
	obj_class->finalize = cimage_finalize;
}

void
cimage_init (CImage *img)
{
	CImagePrivate *priv;

	priv = g_new0(CImagePrivate, 1);

	priv->unique_id = get_unique_id ();
	priv->uri = NULL;
	priv->thumbnail = NULL;
	priv->caption = NULL;
	priv->loading_failed = FALSE;
	priv->is_selected = FALSE;	

	img->priv = priv;
}


CImage*
cimage_new (gchar *text_uri)
{
	CImage *img;
	GnomeVFSURI *uri;

	g_return_val_if_fail (text_uri != NULL, NULL);
	
	uri = gnome_vfs_uri_new (text_uri);
	img = cimage_new_uri (uri);
	gnome_vfs_uri_unref (uri);
	
	return img;
}

CImage*
cimage_new_uri (GnomeVFSURI *uri)
{
	CImage *img;
	
	img = gtk_type_new (cimage_get_type ());
	
	img->priv->uri = uri;
	gnome_vfs_uri_ref (img->priv->uri);
	
	return img;
}

guint      
cimage_get_unique_id (CImage *img)
{
	g_return_val_if_fail (img != NULL, 0);
	return (img->priv->unique_id);
}

GnomeVFSURI*
cimage_get_uri (CImage *img)
{
	g_return_val_if_fail (img != NULL, NULL);
	if (img->priv->uri) {
		gnome_vfs_uri_ref (img->priv->uri);
		return img->priv->uri;
	} else
		return NULL;
}

GdkPixbuf*
cimage_get_thumbnail (CImage *img)
{
	GdkPixbuf *thumb;
	g_return_val_if_fail (img != NULL, FALSE);
	
	thumb = img->priv->thumbnail;
	if (thumb)
		gdk_pixbuf_ref (thumb);

	return thumb;
}

void
cimage_set_loading_failed (CImage *img)
{
	g_return_if_fail (img != NULL);

	img->priv->loading_failed = TRUE;
}

void       
cimage_toggle_select_status (CImage *img)
{
	g_return_if_fail (img != NULL);

	img->priv->is_selected = img->priv->is_selected ? FALSE : TRUE;
}

void       
cimage_set_select_status (CImage *img, gboolean status)
{
	g_return_if_fail (IS_CIMAGE (img));
	
	img->priv->is_selected = status;
}

void
cimage_set_thumbnail (CImage *img, GdkPixbuf *thumbnail)
{
	g_return_if_fail (IS_CIMAGE (img));
	g_return_if_fail (thumbnail != NULL);
	
	if(img->priv->thumbnail)
		gdk_pixbuf_unref (img->priv->thumbnail);

	gdk_pixbuf_ref (thumbnail);
	img->priv->thumbnail = thumbnail;
}

void
cimage_set_caption (CImage *img, gchar *caption)
{
	g_return_if_fail (IS_CIMAGE (img));
	g_return_if_fail (caption != NULL);

	if (img->priv->caption)
		g_free (img->priv->caption);
	
	img->priv->caption = g_strdup (caption);
}

void       
cimage_set_image_dimensions (CImage *img, guint width, guint height)
{
	g_return_if_fail (IS_CIMAGE (img));

	img->priv->width = width;
	img->priv->height = height;
}

gchar*
cimage_get_caption (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), FALSE);

	if (img->priv->caption)
		return g_strdup (img->priv->caption);
	else
		return NULL;
}

gboolean 
cimage_is_directory (CImage *img)
{
	GnomeVFSFileInfo *info;
	gboolean result;

	g_return_val_if_fail (IS_CIMAGE (img), FALSE);

	info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info_uri (img->priv->uri,
				     info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	
	result = (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY);
	gnome_vfs_file_info_unref (info);

	return result;
}

gboolean 
cimage_is_selected (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), FALSE);

	return img->priv->is_selected;
}

gboolean 
cimage_has_thumbnail (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), FALSE);

	return (img->priv->thumbnail != NULL);
}

gboolean
cimage_has_caption (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), FALSE);

	return (img->priv->caption != NULL);
}

gboolean 
cimage_has_loading_failed (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), FALSE);

	return img->priv->loading_failed;
}

guint
cimage_get_width (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), 0);

	return img->priv->width;
}

guint
cimage_get_height (CImage *img)
{
	g_return_val_if_fail (IS_CIMAGE (img), 0);

	return img->priv->height;
}
