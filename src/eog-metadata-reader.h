/* Eye Of GNOME -- Metadata Reader Interface
 *
 * Copyright (C) 2008 The Free Software Foundation
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

#pragma once

#include <glib-object.h>
#ifdef HAVE_EXIF
#include "eog-exif-util.h"
#endif
#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif
#ifdef HAVE_LCMS
#include <lcms2.h>
#endif

G_BEGIN_DECLS

#define EOG_TYPE_METADATA_READER	      (eog_metadata_reader_get_type ())
G_GNUC_INTERNAL
G_DECLARE_INTERFACE (EogMetadataReader, eog_metadata_reader, EOG, METADATA_READER, GObject);

struct _EogMetadataReaderInterface {
	GTypeInterface parent;

	void		(*consume)		(EogMetadataReader *self,
						 const guchar *buf,
						 guint len);

	gboolean	(*finished)		(EogMetadataReader *self);

	void		(*get_raw_exif)		(EogMetadataReader *self,
						 guchar **data,
						 guint *len);

	gpointer	(*get_exif_data)	(EogMetadataReader *self);

	gpointer	(*get_icc_profile)	(EogMetadataReader *self);

	gpointer	(*get_xmp_ptr)		(EogMetadataReader *self);
};

typedef enum {
	EOG_METADATA_JPEG,
	EOG_METADATA_PNG
} EogMetadataFileType;

G_GNUC_INTERNAL
EogMetadataReader*   eog_metadata_reader_new 		(EogMetadataFileType type);

G_GNUC_INTERNAL
void                 eog_metadata_reader_consume	(EogMetadataReader *self,
							 const guchar      *buf,
							 guint              len);

G_GNUC_INTERNAL
gboolean             eog_metadata_reader_finished	(EogMetadataReader *self);

G_GNUC_INTERNAL
void                 eog_metadata_reader_get_exif_chunk (EogMetadataReader  *self,
							 guchar            **data,
							 guint              *len);

#ifdef HAVE_EXIF
G_GNUC_INTERNAL
ExifData*         eog_metadata_reader_get_exif_data	(EogMetadataReader *self);
#endif

#ifdef HAVE_EXEMPI
G_GNUC_INTERNAL
XmpPtr	     	     eog_metadata_reader_get_xmp_data	(EogMetadataReader *self);
#endif

#ifdef HAVE_LCMS
G_GNUC_INTERNAL
cmsHPROFILE          eog_metadata_reader_get_icc_profile (EogMetadataReader *self);
#endif

G_END_DECLS
