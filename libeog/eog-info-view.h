#ifndef _EOG_INFO_VIEW_H_
#define _EOG_INFO_VIEW_H_

#include <gtk/gtk.h>
#include "eog-image.h"

G_BEGIN_DECLS

#define EOG_TYPE_INFO_VIEW             (eog_info_view_get_type ())
#define EOG_INFO_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_INFO_VIEW, EogInfoView))
#define EOG_INFO_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_INFO_VIEW, EogInfoViewClass))
#define EOG_IS_INFO_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_INFO_VIEW))
#define EOG_IS_INFO_VIEW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_INFO_VIEW))

typedef struct _EogInfoView EogInfoView;
typedef struct _EogInfoViewClass EogInfoViewClass;
typedef struct _EogInfoViewPrivate EogInfoViewPrivate;

struct _EogInfoView {
	GtkTreeView widget;

	EogInfoViewPrivate *priv;
};

struct _EogInfoViewClass {
	GtkTreeViewClass parent_class;
};

GType eog_info_view_get_type (void);

void  eog_info_view_set_image (EogInfoView *info, EogImage *image);

#endif /* _EOG_INFO_VIEW_ */
