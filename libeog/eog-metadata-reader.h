#ifndef _EOG_METADATA_READER_H_
#define _EOG_METADATA_READER_H_

#include <config.h>
#include <glib-object.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif 

G_BEGIN_DECLS

#define EOG_TYPE_METADATA_READER            (eog_metadata_reader_get_type ())
#define EOG_METADATA_READER(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_METADATA_READER, EogMetadataReader))
#define EOG_METADATA_READER_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_METADATA_READER, EogMetadataReaderClass))
#define EOG_IS_METADATA_READER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_METADATA_READER))
#define EOG_IS_METADATA_READER_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_METADATA_READER))
#define EOG_METADATA_READER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_METADATA_READER, EogMetadataReaderClass))

typedef struct _EogMetadataReader EogMetadataReader;
typedef struct _EogMetadataReaderClass EogMetadataReaderClass;
typedef struct _EogMetadataReaderPrivate EogMetadataReaderPrivate;

struct _EogMetadataReader {
	GObject parent;

	EogMetadataReaderPrivate *priv;
};

struct _EogMetadataReaderClass {
	GObjectClass parent_klass;
};

typedef enum {
	EOG_METADATA_JPEG
} EogMetadataFileType;

GType               eog_metadata_reader_get_type                       (void) G_GNUC_CONST;

EogMetadataReader*   eog_metadata_reader_new (EogMetadataFileType type);
void                 eog_metadata_reader_consume (EogMetadataReader *emr, guchar *buf, guint len);
gboolean             eog_metadata_reader_finished (EogMetadataReader *emr);

void                 eog_metadata_reader_get_exif_chunk (EogMetadataReader *emr, guchar **data, guint *len);
#if HAVE_EXIF
ExifData*            eog_metadata_reader_get_exif_data (EogMetadataReader *emr);
#endif

#if 0
gpointer             eog_metadata_reader_get_iptc_chunk (EogMetadataReader *emr);
IptcData*            eog_metadata_reader_get_iptc_data (EogMetadataReader *emr);
#endif

G_END_DECLS

#endif /* _EOG_METADATA_READER_H_ */
