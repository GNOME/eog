#ifndef _EOG_PREVIEW_PAGE_H_
#define _EOG_PREVIEW_PAGE_H_

#include <gnome.h>
#include <eog-image-view.h>

BEGIN_GNOME_DECLS

#define EOG_TYPE_PREVIEW_PAGE		 (eog_preview_page_get_type ())
#define EOG_PREVIEW_PAGE(obj)		 (GTK_CHECK_CAST ((obj), EOG_TYPE_PREVIEW_PAGE, EogPreviewPage))
#define EOG_PREVIEW_PAGE_CLASS(klass)	 (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_PREVIEW_PAGE, EogPreviewPageClass))
#define EOG_IS_PREVIEW_PAGE(obj)	 (GTK_CHECK_TYPE ((obj), EOG_TYPE_PREVIEW_PAGE))
#define EOG_IS_PREVIEW_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_PREVIEW_PAGE))

typedef struct _EogPreviewPage		EogPreviewPage;
typedef struct _EogPreviewPagePrivate	EogPreviewPagePrivate;
typedef struct _EogPreviewPageClass	EogPreviewPageClass;

struct _EogPreviewPage
{
	GnomeCanvas		 parent;

	EogPreviewPagePrivate	*priv;
};

struct _EogPreviewPageClass
{
	GnomeCanvasClass	 parent_class;
};

GtkType	   eog_preview_page_get_type (void);
GtkWidget *eog_preview_page_new (EogImageView *image_view, 
				 gint col, gint row);

void 	   eog_preview_page_update (EogPreviewPage *page,
				    GdkPixbuf *pixbuf,
				    gdouble width, gdouble height, 
				    gdouble bottom, gdouble top, 
				    gdouble right, gdouble left, 
				    gboolean vertically, gboolean horizontally,
				    gboolean cut, gdouble overlap_x, 
				    gdouble overlap_y, gboolean overlap,
				    gint *cols_needed, gint *rows_needed);

END_GNOME_DECLS

#endif /* _EOG_PREVIEW_PAGE_H_ */

