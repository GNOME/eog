#include "cimage.h"
#include <gnome.h>

struct _CImagePrivate {
	guint unique_id;

	gchar *path;
	
	GdkPixbuf *thumbnail;

	gchar *caption;	
	
	gboolean  loading_failed;
	gboolean  is_selected;	
};

static GtkObjectClass *parent_class;

static void cimage_class_init (CImageClass *klass);
static void cimage_init (CImage *image);
static void cimage_finalize (GtkObject *obj);
static void cimage_destroy (GtkObject *obj);


#if 0
static guint
get_unique_id (void)
{
	static guint last_id = 0;

	return last_id++;
}
#endif


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

	if (priv->path) {
		g_free (priv->path);
		priv->path = NULL;
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

	priv->unique_id = 0;
	priv->path = NULL;
	priv->thumbnail = NULL;
	priv->caption = NULL;
	priv->loading_failed = FALSE;
	priv->is_selected = FALSE;	

	img->priv = priv;
}


CImage*
cimage_new (gchar *path)
{
	CImage *img;

	g_return_val_if_fail (path != NULL, NULL);
	img = gtk_type_new (cimage_get_type ());
	
	img->priv->path = g_strdup (path);
	
	return img;
}

guint      
cimage_get_unique_id (CImage *img)
{
	g_return_val_if_fail (img != NULL, 0);
	return (img->priv->unique_id);
}

gchar*
cimage_get_path (CImage *img)
{
	g_return_val_if_fail (img != NULL, NULL);
	if (img->priv->path)
		return g_strdup (img->priv->path);
	else
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
cimage_set_thumbnail (CImage *img, GdkPixbuf *thumbnail)
{
	g_return_if_fail (img != NULL);
	g_return_if_fail (thumbnail != NULL);
	
	if(img->priv->thumbnail)
		gdk_pixbuf_unref (img->priv->thumbnail);

	gdk_pixbuf_ref (thumbnail);
	img->priv->thumbnail = thumbnail;
}

void
cimage_set_caption (CImage *img, gchar *caption)
{
	g_return_if_fail (img != NULL);
	g_return_if_fail (caption != NULL);

	if (img->priv->caption)
		g_free (img->priv->caption);
	
	img->priv->caption = g_strdup (caption);
}

gchar*
cimage_get_caption (CImage *img)
{
	g_return_val_if_fail (img != NULL, FALSE);

	if (img->priv->caption)
		return g_strdup (img->priv->caption);
	else
		return NULL;
}

gboolean 
cimage_is_directory (CImage *img)
{
	g_return_val_if_fail (img != NULL, FALSE);
	
	return g_file_test (img->priv->path, G_FILE_TEST_ISDIR);
}

gboolean 
cimage_is_selected (CImage *img)
{
	g_return_val_if_fail (img != NULL, FALSE);
	return img->priv->is_selected;
}

gboolean 
cimage_has_thumbnail (CImage *img)
{
	g_return_val_if_fail (img != NULL, FALSE);
	return (img->priv->thumbnail != NULL);
}

gboolean
cimage_has_caption (CImage *img)
{
	g_return_val_if_fail (img != NULL, FALSE);
	return (img->priv->caption != NULL);
}

gboolean 
cimage_has_loading_failed (CImage *img)
{
	g_return_val_if_fail (img != NULL, FALSE);
	return img->priv->loading_failed;
}

