#ifndef _EOG_METADATA_READER_JPG_H_
#define _EOG_METADATA_READER_JPG_H_

G_BEGIN_DECLS

#define EOG_TYPE_METADATA_READER_JPG            (eog_metadata_reader_jpg_get_type ())
#define EOG_METADATA_READER_JPG(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_METADATA_READER_JPG, EogMetadataReaderJpg))
#define EOG_METADATA_READER_JPG_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_METADATA_READER_JPG, EogMetadataReaderJpgClass))
#define EOG_IS_METADATA_READER_JPG(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_METADATA_READER_JPG))
#define EOG_IS_METADATA_READER_JPG_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_METADATA_READER_JPG))
#define EOG_METADATA_READER_JPG_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_METADATA_READER_JPG, EogMetadataReaderJpgClass))

typedef struct _EogMetadataReaderJpg EogMetadataReaderJpg;
typedef struct _EogMetadataReaderJpgClass EogMetadataReaderJpgClass;
typedef struct _EogMetadataReaderJpgPrivate EogMetadataReaderJpgPrivate;

struct _EogMetadataReaderJpg {
	GObject parent;

	EogMetadataReaderJpgPrivate *priv;
};

struct _EogMetadataReaderJpgClass {
	GObjectClass parent_klass;
};

GType		      eog_metadata_reader_jpg_get_type	(void) G_GNUC_CONST;

EogMetadataReaderJpg* eog_metadata_reader_jpg_new	(EogMetadataFileType type);

G_END_DECLS

#endif /* _EOG_METADATA_READER_JPG_H_ */
