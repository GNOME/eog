#ifndef _EOG_PREVIEW_H_
#define _EOG_PREVIEW_H_

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gconf/gconf-client.h>

BEGIN_GNOME_DECLS

#define EOG_TYPE_PREVIEW	    (eog_preview_get_type ())
#define EOG_PREVIEW(obj)	    (GTK_CHECK_CAST ((obj), EOG_TYPE_PREVIEW, EogPreview))
#define EOG_PREVIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_PREVIEW, EogPreviewClass))
#define EOG_IS_PREVIEW(obj)	    (GTK_CHECK_TYPE ((obj), EOG_TYPE_PREVIEW))
#define EOG_IS_PREVIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_PREVIEW))

typedef struct _EogPreview		EogPreview;
typedef struct _EogPreviewPrivate	EogPreviewPrivate;
typedef struct _EogPreviewClass		EogPreviewClass;

struct _EogPreview
{
	GnomeCanvas		 canvas;
	
	EogPreviewPrivate	*priv;
};

struct _EogPreviewClass
{
	GnomeCanvasClass	 parent_class;
};

GtkType	   eog_preview_get_type (void);

GtkWidget *eog_preview_new (GConfClient *client, GdkPixbuf *pixbuf, 
			    gint col, gint row);

END_GNOME_DECLS

#endif /* _EOG_PREVIEW_H_ */
