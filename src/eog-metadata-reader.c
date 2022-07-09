/* Eye Of GNOME -- Metadata Reader Interface
 *
 * Copyright (C) 2008-2011 The Free Software Foundation
 *
 * Author: Felix Riemann <friemann@svn.gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-metadata-reader.h"
#include "eog-metadata-reader-jpg.h"
#include "eog-metadata-reader-png.h"
#include "eog-debug.h"

G_DEFINE_INTERFACE (EogMetadataReader, eog_metadata_reader, G_TYPE_INVALID)

EogMetadataReader*
eog_metadata_reader_new (EogMetadataFileType type)
{
	EogMetadataReader *self;

	switch (type) {
	case EOG_METADATA_JPEG:
		self = EOG_METADATA_READER (g_object_new (EOG_TYPE_METADATA_READER_JPG, NULL));
		break;
	case EOG_METADATA_PNG:
		self = EOG_METADATA_READER (g_object_new (EOG_TYPE_METADATA_READER_PNG, NULL));
		break;
	default:
		self = NULL;
		break;
	}

	return self;
}

gboolean
eog_metadata_reader_finished (EogMetadataReader *self)
{
	g_return_val_if_fail (EOG_IS_METADATA_READER (self), TRUE);

	return EOG_METADATA_READER_GET_IFACE (self)->finished (self);
}


void
eog_metadata_reader_consume (EogMetadataReader *self, const guchar *buf, guint len)
{
	EOG_METADATA_READER_GET_IFACE (self)->consume (self, buf, len);
}

/* Returns the raw exif data. NOTE: The caller of this function becomes
 * the new owner of this piece of memory and is responsible for freeing it!
 */
void
eog_metadata_reader_get_exif_chunk (EogMetadataReader *self, guchar **data, guint *len)
{
	g_return_if_fail (data != NULL && len != NULL);

	EOG_METADATA_READER_GET_IFACE (self)->get_raw_exif (self, data, len);
}

#ifdef HAVE_EXIF
ExifData*
eog_metadata_reader_get_exif_data (EogMetadataReader *self)
{
	return EOG_METADATA_READER_GET_IFACE (self)->get_exif_data (self);
}
#endif

#ifdef HAVE_EXEMPI
XmpPtr
eog_metadata_reader_get_xmp_data (EogMetadataReader *self)
{
	return EOG_METADATA_READER_GET_IFACE (self)->get_xmp_ptr (self);
}
#endif

#ifdef HAVE_LCMS
cmsHPROFILE
eog_metadata_reader_get_icc_profile (EogMetadataReader *self)
{
	return EOG_METADATA_READER_GET_IFACE (self)->get_icc_profile (self);
}
#endif

/* Default vfunc that simply clears the output if not overridden by the
   implementing class. This mimics the old behaviour of get_exif_chunk(). */
static void
_eog_metadata_reader_default_get_raw_exif (EogMetadataReader *self,
					   guchar **data, guint *length)
{
	g_return_if_fail (data != NULL && length != NULL);

	*data = NULL;
	*length = 0;
}

/* Default vfunc that simply returns NULL if not overridden by the implementing
   class. Mimics the old fallback behaviour of the getter functions. */
static gpointer
_eog_metadata_reader_default_get_null (EogMetadataReader *self)
{
	return NULL;
}

static void
eog_metadata_reader_default_init (EogMetadataReaderInterface *iface)
{
	/* consume and finished are required to be implemented */
	/* Not-implemented funcs return NULL by default */
	iface->get_raw_exif = _eog_metadata_reader_default_get_raw_exif;
	iface->get_exif_data = _eog_metadata_reader_default_get_null;
	iface->get_icc_profile = _eog_metadata_reader_default_get_null;
	iface->get_xmp_ptr = _eog_metadata_reader_default_get_null;
}
