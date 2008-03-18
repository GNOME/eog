#ifndef _EOG_METADATA_READER_H_
#define _EOG_METADATA_READER_H_

#include <glib-object.h>
#if HAVE_EXIF
#include <libexif/exif-data.h>
#endif 
#if HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

G_BEGIN_DECLS

#define EOG_TYPE_METADATA_READER	      (eog_metadata_reader_get_type ())
#define EOG_METADATA_READER(o)		      (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_METADATA_READER, EogMetadataReader))
#define EOG_IS_METADATA_READER(o)	      (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_METADATA_READER))
#define EOG_METADATA_READER_GET_INTERFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), EOG_TYPE_METADATA_READER, EogMetadataReaderInterface))

typedef struct _EogMetadataReader EogMetadataReader;
typedef struct _EogMetadataReaderInterface EogMetadataReaderInterface;

struct _EogMetadataReaderInterface {
	GTypeInterface parent;

	void		(*consume) 	(EogMetadataReader *self,
					 const guchar *buf,
					 guint len);

	gboolean	(*finished) 	(EogMetadataReader *self);

	void		(*get_raw_exif) (EogMetadataReader *self,
					 guchar **data,
					 guint *len);

	gpointer	(*get_exif_data) (EogMetadataReader *self);

	void		(*get_icc_chunk) (EogMetadataReader *self,
					  guchar **data,
					  guint *len);

	gpointer	(*get_xmp_ptr) 	(EogMetadataReader *self);
};

typedef enum {
	EOG_METADATA_JPEG
} EogMetadataFileType;

GType                eog_metadata_reader_get_type (void) G_GNUC_CONST;

EogMetadataReader*   eog_metadata_reader_new (EogMetadataFileType type);
void                 eog_metadata_reader_consume (EogMetadataReader *emr, const guchar *buf, guint len);
gboolean             eog_metadata_reader_finished (EogMetadataReader *emr);

void                 eog_metadata_reader_get_exif_chunk (EogMetadataReader *emr, guchar **data, guint *len);

#ifdef HAVE_EXIF
ExifData*            eog_metadata_reader_get_exif_data (EogMetadataReader *emr);
#endif

#ifdef HAVE_EXEMPI
XmpPtr	     	     eog_metadata_reader_get_xmp_data (EogMetadataReader *emr);
#endif

#if 0
gpointer             eog_metadata_reader_get_iptc_chunk (EogMetadataReader *emr);
IptcData*            eog_metadata_reader_get_iptc_data (EogMetadataReader *emr);
#endif

void                 eog_metadata_reader_get_icc_chunk (EogMetadataReader *emr, guchar **data, guint *len);

G_END_DECLS

#endif /* _EOG_METADATA_READER_H_ */
