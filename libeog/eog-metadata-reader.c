#include <string.h>
#include <libgnome/gnome-macros.h>

#include "eog-metadata-reader.h"

typedef enum {
	EMR_READ = 0,
	EMR_READ_SIZE_HIGH_BYTE,
	EMR_READ_SIZE_LOW_BYTE,
	EMR_READ_MARKER,
	EMR_SKIP_BYTES,
	EMR_READ_EXIF,
	EMR_READ_IPTC,
	EMR_FINISHED
} EogMetadataReaderState;

#define EOG_JPEG_MARKER_START   0xFF
#define EOG_JPEG_MARKER_SOI     0xD8
#define EOG_JPEG_MARKER_APP1	0xE1
#define EOG_JPEG_MARKER_APP14	0xED

struct _EogMetadataReaderPrivate {
	EogMetadataReaderState  state;

	/* data fields */
	gpointer exif_chunk;
	guint    exif_len;
	
	gpointer iptc_chunk;
	guint	 iptc_len;
	
	/* management fields */
	int      size;
	int      last_marker;
	int      bytes_read;	
};

static void
eog_metadata_reader_finalize (GObject *object)
{
	EogMetadataReader *instance = EOG_METADATA_READER (object);
	
	g_free (instance->priv);
	instance->priv = NULL;
}

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
}

static void
eog_metadata_reader_instance_init (EogMetadataReader *obj)
{
	EogMetadataReaderPrivate *priv;

	priv = g_new0 (EogMetadataReaderPrivate, 1);
	priv->exif_chunk = NULL;
	priv->exif_len = 0;
	priv->iptc_chunk = NULL;
	priv->iptc_len = 0;
	
	obj->priv = priv;
}

static void 
eog_metadata_reader_class_init (EogMetadataReaderClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_metadata_reader_finalize;
	object_class->dispose = eog_metadata_reader_dispose;
}


GNOME_CLASS_BOILERPLATE (EogMetadataReader,
			 eog_metadata_reader,
			 GObject,
			 G_TYPE_OBJECT);

EogMetadataReader*
eog_metadata_reader_new (EogMetadataFileType type)
{
	EogMetadataReader *emr;
	
	/* CAUTION: check for type if we support more metadat-image-formats in the future */
	
	emr = g_object_new (EOG_TYPE_METADATA_READER, NULL);	
	return emr;
}

void
eog_metadata_reader_consume (EogMetadataReader *emr, guchar *buf, guint len)
{
	EogMetadataReaderPrivate *priv;
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
				g_print ("APPx marker found: %x\n", priv->last_marker);
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
			}
			else if (priv->last_marker == EOG_JPEG_MARKER_APP1 && 
				 priv->exif_chunk == NULL) 
			{
				priv->state = EMR_READ_EXIF;
			}
			else if (priv->last_marker == EOG_JPEG_MARKER_APP14 && 
				priv->iptc_chunk == NULL) 
			{
				priv->state = EMR_READ_IPTC;
			}
			else {
				priv->state = EMR_SKIP_BYTES;
			}

			priv->last_marker = 0;
			break;			
			
		case EMR_SKIP_BYTES:
			g_print ("skip bytes: %i\n", priv->size);
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
			
		case EMR_READ_EXIF:			
			g_print ("Read EXIF data, length: %i\n", priv->size);
			if (priv->exif_chunk == NULL) { 
				priv->exif_chunk = g_new0 (guchar, priv->size);
				priv->exif_len = priv->size;
				priv->bytes_read = 0;
			}

			if (i + priv->size < len) {
                                /* read data in one block */
				memcpy (priv->exif_chunk + priv->bytes_read, &buf[i], priv->size); 
				priv->state = EMR_READ;
				i = i + priv->size - 1; /* the for-loop consumes the other byte */
			}
			else {
				int chunk_len = len - i;
				memcpy (priv->exif_chunk + priv->bytes_read, &buf[i], chunk_len);
				priv->bytes_read += chunk_len; /* bytes already read */
				priv->size = (i + priv->size) - len; /* remaining data to read */
				i = len - 1;
				priv->state = EMR_READ_EXIF;
			}
			
			if (priv->exif_chunk != NULL && priv->iptc_chunk != NULL)
				priv->state = EMR_FINISHED;
			break;
			
		case EMR_READ_IPTC:
			if (priv->iptc_chunk == NULL) { 
				priv->iptc_chunk = g_new0 (guchar, priv->size);
				priv->iptc_len = priv->size;
				priv->bytes_read = 0;
			}

			if (i + priv->size < len) {
                                /* read data in one block */
				memcpy (priv->iptc_chunk + priv->bytes_read, &buf[i], priv->size); 
				priv->state = EMR_READ;
			}
			else {
				int chunk_len = len - i;
				memcpy (priv->iptc_chunk + priv->bytes_read, &buf[i], chunk_len);
				priv->bytes_read += chunk_len; /* bytes already read */
				priv->size = (i + priv->size) - len; /* remaining data to read */
				i = len - 1;
				priv->state = EMR_READ_IPTC;
			}
			
			if (priv->exif_chunk != NULL && priv->iptc_chunk != NULL)
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

#if HAVE_EXIF
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
