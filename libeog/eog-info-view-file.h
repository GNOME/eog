#ifndef _EOG_INFO_VIEW_FILE_H_
#define _EOG_INFO_VIEW_FILE_H_

#include <glib-object.h>
#include "eog-info-view-detail.h"
#include "eog-image.h"

G_BEGIN_DECLS

#define EOG_TYPE_INFO_VIEW_FILE            (eog_info_view_file_get_type ())
#define EOG_INFO_VIEW_FILE(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_INFO_VIEW_FILE, EogInfoViewFile))
#define EOG_INFO_VIEW_FILE_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_INFO_VIEW_FILE, EogInfoViewFileClass))
#define EOG_IS_INFO_VIEW_FILE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_INFO_VIEW_FILE))
#define EOG_IS_INFO_VIEW_FILE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_INFO_VIEW_FILE))
#define EOG_INFO_VIEW_FILE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_INFO_VIEW_FILE, EogInfoViewFileClass))

typedef struct _EogInfoViewFile EogInfoViewFile;
typedef struct _EogInfoViewFileClass EogInfoViewFileClass;
typedef struct _EogInfoViewFilePrivate EogInfoViewFilePrivate;

struct _EogInfoViewFile {
	EogInfoViewDetail parent;

	EogInfoViewFilePrivate *priv;
};

struct _EogInfoViewFileClass {
	EogInfoViewDetailClass parent_klass;
};

GType               eog_info_view_file_get_type                       (void) G_GNUC_CONST;

void                eog_info_view_file_show_data                      (EogInfoViewFile *view, EogImage *image);

G_END_DECLS

#endif /* _EOG_INFO_VIEW_FILE_H_ */
