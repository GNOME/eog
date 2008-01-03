#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "eog-metadata-reader.h"
#include "eog-debug.h"

typedef enum {
	EMR_READ = 0,
	EMR_READ_SIZE_HIGH_BYTE,
	EMR_READ_SIZE_LOW_BYTE,
	EMR_READ_MARKER,
	EMR_SKIP_BYTES,
	EMR_READ_APP1,
	EMR_READ_EXIF,
	EMR_READ_XMP,
	EMR_READ_ICC,
	EMR_READ_IPTC,
	EMR_FINISHED
} EogMetadataReaderState;

typedef enum {
	EJA_EXIF = 0,
	EJA_XMP,
	EJA_OTHER
} EogJpegApp1Type;


#define EOG_JPEG_MARKER_START   0xFF
#define EOG_JPEG_MARKER_SOI     0xD8
#define EOG_JPEG_MARKER_APP1	0xE1
#define EOG_JPEG_MARKER_APP2	0xE2
#define EOG_JPEG_MARKER_APP14	0xED

#define NO_DEBUG

#define IS_FINISHED(priv) (priv->exif_chunk != NULL && \
                           priv->icc_chunk  != NULL && \
                           priv->iptc_chunk != NULL && \
                           priv->xmp_chunk  != NULL)

struct _EogMetadataReaderPrivate {
	EogMetadataReaderState  state;

	/* data fields */
	gpointer exif_chunk;
	guint    exif_len;
	
	gpointer iptc_chunk;
	guint	 iptc_len;
	
	gpointer icc_chunk;
	guint icc_len;

	gpointer xmp_chunk;
	guint xmp_len;
	
	/* management fields */
	int      size;
	int      last_marker;
	int      bytes_read;	
};

#define EOG_METADATA_READER_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_METADATA_READER, EogMetadataReaderPrivate))

G_DEFINE_TYPE (EogMetadataReader, eog_metadata_reader, G_TYPE_OBJECT)

static void
eog_metadata_reader_dispose (GObject *object)
{
	EogMetadataReader *emr = EOG_METADATA_READER (object);
	
	if (emr->priv->exif_chunk != NULL) {
		g_free (emr->priv->exif_chunk);
		emr->priv->exif_chunk = NULL;
	}

	if (emr->priv->iptc_chunk != NULL) {
		g_free (emr->priv->iptc_chunk);
		emr->priv->iptc_chunk = NULL;
	}

	if (emr->priv->xmp_chunk != NULL) {
		g_free (emr->priv->xmp_chunk);
		emr->priv->xmp_chunk = NULL;
	}

	if (emr->priv->icc_chunk != NULL) {
		g_free (emr->priv->icc_chunk);
		emr->priv->icc_chunk = NULL;
	}

	G_OBJECT_CLASS (eog_metadata_reader_parent_class)->dispose (object);
}

static void
eog_metadata_reader_init (EogMetadataReader *obj)
{
	EogMetadataReaderPrivate *priv;

	priv = obj->priv =  EOG_METADATA_READER_GET_PRIVATE (obj);
	priv->exif_chunk = NULL;
	priv->exif_len = 0;
	priv->iptc_chunk = NULL;
	priv->iptc_len = 0;
	priv->icc_chunk = NULL;
	priv->icc_len = 0;
}

static void 
eog_metadata_reader_class_init (EogMetadataReaderClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->dispose = eog_metadata_reader_dispose;

	g_type_class_add_private (klass, sizeof (EogMetadataReaderPrivate));
}

EogMetadataReader*
eog_metadata_reader_new (EogMetadataFileType type)
{
	EogMetadataReader *emr;
	
	/* CAUTION: check for type if we support more metadat-image-formats in the future */
	
	emr = g_object_new (EOG_TYPE_METADATA_READER, NULL);	
	return emr;
}

gboolean
eog_metadata_reader_finished (EogMetadataReader *emr)
{
	g_return_val_if_fail (EOG_IS_METADATA_READER (emr), TRUE);

	return (emr->priv->state == EMR_FINISHED);
}


static EogJpegApp1Type
eog_metadata_identify_app1 (gchar *buf, guint len)
{
 	if (len < 5) {
 		return EJA_OTHER;
 	}

 	if (len < 29) {
 		return (strncmp ("Exif", buf, 5) == 0 ? EJA_EXIF : EJA_OTHER);
 	}

 	if (strncmp ("Exif", buf, 5) == 0) {
 		return EJA_EXIF;
 	} else if (strncmp ("http://ns.adobe.com/xap/1.0/", buf, 29) == 0) {
 		return EJA_XMP;
 	}

 	return EJA_OTHER;
}

static void
eog_metadata_reader_get_next_block (EogMetadataReaderPrivate* priv,
				    guchar *chunk,
				    int* i,
				    guchar *buf,
				    int len,
				    EogMetadataReaderState state)
{
	if (*i + priv->size < len) {
		/* read data in one block */
		memcpy ((guchar*) (chunk) + priv->bytes_read, &buf[*i], priv->size);
		priv->state = EMR_READ;
		*i = *i + priv->size - 1; /* the for-loop consumes the other byte */
	} else {
		int chunk_len = len - *i;
		memcpy ((guchar*) (chunk) + priv->bytes_read, &buf[*i], chunk_len);
		priv->bytes_read += chunk_len; /* bytes already read */
		priv->size = (*i + priv->size) - len; /* remaining data to read */
		*i = len - 1;
		priv->state = state;
	}
}

void
eog_metadata_reader_consume (EogMetadataReader *emr, guchar *buf, guint len)
{
	EogMetadataReaderPrivate *priv;
 	EogJpegApp1Type app1_type;
	int i;

	g_return_if_fail (EOG_IS_METADATA_READER (emr));

	priv = emr->priv;
	
	if (priv->state == EMR_FINISHED) return;

	for (i = 0; (i < len) && (priv->state != EMR_FINISHED); i++) {

		switch (priv->state) {
		case EMR_READ:
			if (buf[i] == EOG_JPEG_MARKER_START) {
				priv->state = EMR_READ_MARKER;
			}
			else {
				priv->state = EMR_FINISHED;
			}
			break;

		case EMR_READ_MARKER:
			if ((buf [i] & 0xF0) == 0xE0) { /* we are reading some sort of APPxx marker */
				/* these are always followed by 2 bytes of size information */
				priv->last_marker = buf [i];
				priv->size = 0;
				priv->state = EMR_READ_SIZE_HIGH_BYTE;

				eog_debug_message (DEBUG_IMAGE_DATA, "APPx Marker Found: %x", priv->last_marker);
			}
			else {
				/* otherwise simply consume the byte */
				priv->state = EMR_READ;
			}
			break;
			
		case EMR_READ_SIZE_HIGH_BYTE:
			priv->size = (buf [i] & 0xff) << 8;
			priv->state = EMR_READ_SIZE_LOW_BYTE;
			break;			
			
		case EMR_READ_SIZE_LOW_BYTE:
			priv->size |= (buf [i] & 0xff);			
			
			if (priv->size > 2)  /* ignore the two size-bytes */
				priv->size -= 2;
		
			if (priv->size == 0) {
				priv->state = EMR_READ;
			} else if (priv->last_marker == EOG_JPEG_MARKER_APP1 && 
				   ((priv->exif_chunk == NULL) || (priv->xmp_chunk == NULL))) 
			{
				priv->state = EMR_READ_APP1;
			} else if (priv->last_marker == EOG_JPEG_MARKER_APP2 && 
				 priv->icc_chunk == NULL) 
			{
				priv->state = EMR_READ_ICC;
			} else if (priv->last_marker == EOG_JPEG_MARKER_APP14 && 
				priv->iptc_chunk == NULL) 
			{
				priv->state = EMR_READ_IPTC;
			} else {
				priv->state = EMR_SKIP_BYTES;
			}

			priv->last_marker = 0;
			break;			
			
		case EMR_SKIP_BYTES:
			eog_debug_message (DEBUG_IMAGE_DATA, "Skip bytes: %i", priv->size);

			if (i + priv->size < len) { 
				i = i + priv->size - 1; /* the for-loop consumes the other byte */
				priv->size = 0;
			}
			else {  
				priv->size = (i + priv->size) - len;
				i = len - 1;
			}
			if (priv->size == 0) { /* don't need to skip any more bytes */
				priv->state = EMR_READ;
			}
			break;
			
		case EMR_READ_APP1:			
			eog_debug_message (DEBUG_IMAGE_DATA, "Read APP1 data, Length: %i", priv->size);

			app1_type = eog_metadata_identify_app1 ((gchar*) &buf[i], priv->size);
			
			EogMetadataReaderState next_state;
			guchar *chunk = NULL;
			int offset = 0;

			switch (app1_type) {
			case EJA_EXIF:
				if (priv->exif_chunk == NULL) { 
					priv->exif_chunk = g_new0 (guchar, priv->size);
					priv->exif_len = priv->size;
					priv->bytes_read = 0;
					chunk = priv->exif_chunk;
					next_state = EMR_READ_EXIF;
				}
				break;
			case EJA_XMP:
				if (priv->xmp_chunk == NULL) { 
					offset = 29 + 54; /* skip the ID + packet */
					if (priv->size > offset)  { /* ensure that we have enough bytes */
						priv->xmp_chunk = g_new0 (guchar, priv->size);
						priv->xmp_len = priv->size - offset;
						priv->bytes_read = 0;
						chunk = priv->xmp_chunk;
						next_state = EMR_READ_XMP;
					}
				}
				break;
			case EJA_OTHER:
			default:
				/* skip unknown data */
				priv->state = EMR_SKIP_BYTES;
				break;
			}

			if (chunk) {
				if (i + priv->size < len) {
					/* read data in one block */
					memcpy ((guchar*) (chunk) + priv->bytes_read, &buf[i + offset], priv->size);
					priv->state = EMR_READ;
					i = i + priv->size - 1; /* the for-loop consumes the other byte */
				} else {
					int chunk_len = len - i;
					memcpy ((guchar*) (priv->exif_chunk) + priv->bytes_read, &buf[i], chunk_len);
					priv->bytes_read += chunk_len; /* bytes already read */
					priv->size = (i + priv->size) - len; /* remaining data to read */
					i = len - 1;
					priv->state = next_state;
				}
			}

			if (IS_FINISHED(priv))
				priv->state = EMR_FINISHED;
			break;
			
		case EMR_READ_EXIF:                     
			eog_debug_message (DEBUG_IMAGE_DATA, "Read continuation of EXIF data, length: %i", priv->size);
			{
 				eog_metadata_reader_get_next_block (priv, priv->exif_chunk,
 								    &i, buf, len, EMR_READ_EXIF);
			}
			if (IS_FINISHED(priv))
				priv->state = EMR_FINISHED;
			break;
			
		case EMR_READ_XMP:
			eog_debug_message (DEBUG_IMAGE_DATA, "Read continuation of XMP data, length: %i", priv->size);
			{
				eog_metadata_reader_get_next_block (priv, priv->xmp_chunk,
 								    &i, buf, len, EMR_READ_XMP);
			}
			if (IS_FINISHED (priv))
				priv->state = EMR_FINISHED;
			break;
			
		case EMR_READ_ICC:			
			eog_debug_message (DEBUG_IMAGE_DATA,
					   "Read continuation of ICC data, "
					   "length: %i", priv->size);

			if (priv->icc_chunk == NULL) { 
				priv->icc_chunk = g_new0 (guchar, priv->size);
				priv->icc_len = priv->size;
				priv->bytes_read = 0;
			}

			eog_metadata_reader_get_next_block (priv, priv->icc_chunk,
							    &i, buf, len, EMR_READ_ICC);
			
			if (IS_FINISHED(priv))
				priv->state = EMR_FINISHED;
			break;
			
		case EMR_READ_IPTC:
			eog_debug_message (DEBUG_IMAGE_DATA,
					   "Read continuation of IPTC data, "
					   "length: %i", priv->size);

			if (priv->iptc_chunk == NULL) { 
				priv->iptc_chunk = g_new0 (guchar, priv->size);
				priv->iptc_len = priv->size;
				priv->bytes_read = 0;
			}

			eog_metadata_reader_get_next_block (priv,
							    priv->iptc_chunk,
							    &i, buf, len,
							    EMR_READ_IPTC);
			
			if (IS_FINISHED(priv))
				priv->state = EMR_FINISHED;
			break;

		default:
			g_assert_not_reached ();
		}
	}
}

/* Returns the raw exif data. NOTE: The caller of this function becomes
 * the new owner of this piece of memory and is responsible for freeing it! 
 */
void
eog_metadata_reader_get_exif_chunk (EogMetadataReader *emr, guchar **data, guint *len)
{
	EogMetadataReaderPrivate *priv;
	
	g_return_if_fail (EOG_IS_METADATA_READER (emr));
	priv = emr->priv;
	
	*data = (guchar*) priv->exif_chunk;
	*len = priv->exif_len;
	
	priv->exif_chunk = NULL;
	priv->exif_len = 0;
}

#ifdef HAVE_EXIF
ExifData*
eog_metadata_reader_get_exif_data (EogMetadataReader *emr)
{
	EogMetadataReaderPrivate *priv;
	ExifData *data = NULL;
	
	g_return_val_if_fail (EOG_IS_METADATA_READER (emr), NULL);
	priv = emr->priv;
	
	if (priv->exif_chunk != NULL) {
		data = exif_data_new_from_data (priv->exif_chunk, priv->exif_len);
	}
	
	return data;
}
#endif


#ifdef HAVE_EXEMPI
XmpPtr 
eog_metadata_reader_get_xmp_data (EogMetadataReader *emr )
{
	EogMetadataReaderPrivate *priv;
	XmpPtr xmp = NULL;

	g_return_val_if_fail (EOG_IS_METADATA_READER (emr), NULL);

	priv = emr->priv;

	if (priv->xmp_chunk != NULL) {
		xmp = xmp_new (priv->xmp_chunk, priv->xmp_len);
	}

	return xmp;
}
#endif

/*
 * FIXME: very broken, assumes the profile fits in a single chunk.  Change to
 * parse the sections and construct a single memory chunk, or maybe even parse
 * the profile.
 */
void
eog_metadata_reader_get_icc_chunk (EogMetadataReader *emr, guchar **data, guint *len)
{
	EogMetadataReaderPrivate *priv;
	
	g_return_if_fail (EOG_IS_METADATA_READER (emr));

	priv = emr->priv;

	if (priv->icc_chunk) {	
		*data = (guchar*) priv->icc_chunk + 14;
		*len = priv->icc_len - 14;
	}
}
