#ifndef _EOG_INFO_VIEW_DETAIL_H_
#define _EOG_INFO_VIEW_DETAIL_H_

#include <glib-object.h>
#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

#define EOG_TYPE_INFO_VIEW_DETAIL            (eog_info_view_detail_get_type ())
#define EOG_INFO_VIEW_DETAIL(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_INFO_VIEW_DETAIL, EogInfoViewDetail))
#define EOG_INFO_VIEW_DETAIL_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_INFO_VIEW_DETAIL, EogInfoViewDetailClass))
#define EOG_IS_INFO_VIEW_DETAIL(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_INFO_VIEW_DETAIL))
#define EOG_IS_INFO_VIEW_DETAIL_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_INFO_VIEW_DETAIL))
#define EOG_INFO_VIEW_DETAIL_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_INFO_VIEW_DETAIL, EogInfoViewDetailClass))

typedef struct _EogInfoViewDetail EogInfoViewDetail;
typedef struct _EogInfoViewDetailClass EogInfoViewDetailClass;
typedef struct _EogInfoViewDetailPrivate EogInfoViewDetailPrivate;

struct _EogInfoViewDetail {
	GtkTreeView parent;

	EogInfoViewDetailPrivate *priv;
};

struct _EogInfoViewDetailClass {
	GtkTreeViewClass parent_klass;
};

GType               eog_info_view_detail_get_type                       (void) G_GNUC_CONST;

#if 0
/* These are protected functions, which are only useful for subclasses. */

char*               _eog_info_view_detail_set_row_data                  (EogInfoViewDetail *view, 
									 char *path, 
									 char *parent, 
									 const char *attribute, 
									 const char *value);
#endif

void                _eog_info_view_detail_clear_values                  (EogInfoViewDetail *view);

G_END_DECLS

#endif /* _EOG_INFO_VIEW_DETAIL_H_ */
