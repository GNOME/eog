/* Eye Of Gnome - EOG Metadata Details
 *
 * Copyright (C) 2006 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
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
#include <gtk/gtk.h>
#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#endif
#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

G_BEGIN_DECLS

typedef struct _EogMetadataDetails EogMetadataDetails;
typedef struct _EogMetadataDetailsClass EogMetadataDetailsClass;
typedef struct _EogMetadataDetailsPrivate EogMetadataDetailsPrivate;

#define EOG_TYPE_METADATA_DETAILS            (eog_metadata_details_get_type ())
#define EOG_METADATA_DETAILS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_METADATA_DETAILS, EogMetadataDetails))
#define EOG_METADATA_DETAILS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EOG_TYPE_METADATA_DETAILS, EogMetadataDetailsClass))
#define EOG_IS_METADATA_DETAILS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_METADATA_DETAILS))
#define EOG_IS_METADATA_DETAILS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_METADATA_DETAILS))
#define EOG_METADATA_DETAILS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EOG_TYPE_METADATA_DETAILS, EogMetadataDetailsClass))

struct _EogMetadataDetails {
        GtkTreeView parent;

        EogMetadataDetailsPrivate *priv;
};

struct _EogMetadataDetailsClass {
	GtkTreeViewClass parent_class;
};

G_GNUC_INTERNAL
GType               eog_metadata_details_get_type    (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkWidget          *eog_metadata_details_new         (void);

#ifdef HAVE_EXIF
G_GNUC_INTERNAL
void                eog_metadata_details_update      (EogMetadataDetails *details,
						      ExifData       *data);
#endif
#ifdef HAVE_EXEMPI
G_GNUC_INTERNAL
void                eog_metadata_details_xmp_update  (EogMetadataDetails *view,
						      XmpPtr          xmp_data);
#endif

G_END_DECLS
