#ifndef _EOG_PREVIEW_H_
#define _EOG_PREVIEW_H_

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <eog-image-view.h>

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
	GtkHBox			 parent; 
	
	EogPreviewPrivate	*priv;
};

struct _EogPreviewClass
{
	GtkHBoxClass		  parent_class;
};

GtkType	   eog_preview_get_type (void);
GtkWidget *eog_preview_new (GConfClient *client, EogImageView *image_view);

END_GNOME_DECLS

#endif /* _EOG_PREVIEW_H_ */
