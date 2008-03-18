#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "eog-metadata-reader.h"
#include "eog-metadata-reader-jpg.h"
#include "eog-debug.h"


GType
eog_metadata_reader_get_type (void)
{
	static GType reader_type = 0;

	if (G_UNLIKELY (reader_type == 0)) {
		reader_type = g_type_register_static_simple (G_TYPE_INTERFACE,
							     "EogMetadataReader",
							     sizeof (EogMetadataReaderInterface),
							     NULL, 0, NULL, 0);
	}

	return reader_type;
}

EogMetadataReader*
eog_metadata_reader_new (EogMetadataFileType type)
{
	EogMetadataReader *emr = NULL;
	
	/* CAUTION: check for type if we support more metadata-image-formats in the future */
	if (type == EOG_METADATA_JPEG)
		emr = EOG_METADATA_READER (eog_metadata_reader_jpg_new (type));

	return emr;
}

gboolean
eog_metadata_reader_finished (EogMetadataReader *emr)
{
	g_return_val_if_fail (EOG_IS_METADATA_READER (emr), TRUE);

	return EOG_METADATA_READER_GET_INTERFACE (emr)->finished (emr);
}


void
eog_metadata_reader_consume (EogMetadataReader *emr, const guchar *buf, guint len)
{
	EOG_METADATA_READER_GET_INTERFACE (emr)->consume (emr, buf, len);
}

/* Returns the raw exif data. NOTE: The caller of this function becomes
 * the new owner of this piece of memory and is responsible for freeing it! 
 */
void
eog_metadata_reader_get_exif_chunk (EogMetadataReader *emr, guchar **data, guint *len)
{
	EogMetadataReaderInterface *iface;

	g_return_if_fail (data != NULL && len != NULL);
	iface = EOG_METADATA_READER_GET_INTERFACE (emr);
	
	if (iface->get_raw_exif) {
		iface->get_raw_exif (emr, data, len);
	} else {
		g_return_if_fail (data != NULL && len != NULL);

		*data = NULL;
		*len = 0;
	}

}

#ifdef HAVE_EXIF
ExifData*
eog_metadata_reader_get_exif_data (EogMetadataReader *emr)
{
	gpointer exif_data = NULL;
	EogMetadataReaderInterface *iface;

	iface = EOG_METADATA_READER_GET_INTERFACE (emr);
	if (iface->get_exif_data)
		exif_data = iface->get_exif_data (emr);
	
	return exif_data;
}
#endif

#ifdef HAVE_EXEMPI
XmpPtr
eog_metadata_reader_get_xmp_data (EogMetadataReader *emr )
{
	gpointer xmp_data = NULL;
	EogMetadataReaderInterface *iface;

	iface = EOG_METADATA_READER_GET_INTERFACE (emr);
	
	if (iface->get_xmp_ptr)
		xmp_data = iface->get_xmp_ptr (emr);

	return xmp_data;
}
#endif

void
eog_metadata_reader_get_icc_chunk (EogMetadataReader *emr, guchar **data, guint *len)
{
	EogMetadataReaderInterface *iface;

	g_return_if_fail (data != NULL && len != NULL);

	iface = EOG_METADATA_READER_GET_INTERFACE (emr);
	
	if (iface->get_icc_chunk)
		iface->get_icc_chunk (emr, data, len);
	else {
		*data = NULL;
		*len = 0;
	}
}
