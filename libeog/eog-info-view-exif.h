#ifndef _EOG_INFO_VIEW_EXIF_H_
#define _EOG_INFO_VIEW_EXIF_H_

#include <config.h>

#if HAVE_EXIF

#include <glib-object.h>
#include <libexif/exif-data.h>

#include "eog-info-view-detail.h"

G_BEGIN_DECLS

#define EOG_TYPE_INFO_VIEW_EXIF            (eog_info_view_exif_get_type ())
#define EOG_INFO_VIEW_EXIF(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_INFO_VIEW_EXIF, EogInfoViewExif))
#define EOG_INFO_VIEW_EXIF_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_INFO_VIEW_EXIF, EogInfoViewExifClass))
#define EOG_IS_INFO_VIEW_EXIF(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_INFO_VIEW_EXIF))
#define EOG_IS_INFO_VIEW_EXIF_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_INFO_VIEW_EXIF))
#define EOG_INFO_VIEW_EXIF_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_INFO_VIEW_EXIF, EogInfoViewExifClass))

typedef struct _EogInfoViewExif EogInfoViewExif;
typedef struct _EogInfoViewExifClass EogInfoViewExifClass;
typedef struct _EogInfoViewExifPrivate EogInfoViewExifPrivate;

struct _EogInfoViewExif {
        EogInfoViewDetail parent;

	EogInfoViewExifPrivate *priv;
};

struct _EogInfoViewExifClass {
	EogInfoViewDetailClass parent_klass;
};

GType               eog_info_view_exif_get_type                       (void) G_GNUC_CONST;

void                eog_info_view_exif_show_data                      (EogInfoViewExif *view, ExifData *data);

G_END_DECLS

#endif /* HAVE_EXIF */

#endif /* _EOG_INFO_VIEW_EXIF_H_ */

