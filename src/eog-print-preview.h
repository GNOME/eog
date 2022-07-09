/* Eye of GNOME -- Print Preview Widget
 *
 * Copyright (C) 2006-2007 The Free Software Foundation
 *
 * Author: Claudio Saavedra <csaavedra@alumnos.utalca.cl>
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

G_BEGIN_DECLS

typedef struct _EogPrintPreview EogPrintPreview;
typedef struct _EogPrintPreviewClass EogPrintPreviewClass;
typedef struct _EogPrintPreviewPrivate EogPrintPreviewPrivate;

#define EOG_TYPE_PRINT_PREVIEW            (eog_print_preview_get_type ())
#define EOG_PRINT_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_PRINT_PREVIEW, EogPrintPreview))
#define EOG_PRINT_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_PRINT_PREVIEW, EogPrintPreviewClass))
#define EOG_IS_PRINT_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_PRINT_PREVIEW))
#define EOG_IS_PRINT_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_PRINT_PREVIEW))

struct _EogPrintPreview {
	GtkAspectFrame aspect_frame;

	EogPrintPreviewPrivate *priv;
};

struct _EogPrintPreviewClass {
	GtkAspectFrameClass parent_class;

};

G_GNUC_INTERNAL
GType        eog_print_preview_get_type            (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkWidget   *eog_print_preview_new                 (void);

G_GNUC_INTERNAL
GtkWidget   *eog_print_preview_new_with_pixbuf     (GdkPixbuf       *pixbuf);

G_GNUC_INTERNAL
void         eog_print_preview_set_page_margins    (EogPrintPreview *preview,
						    gfloat          l_margin,
						    gfloat          r_margin,
						    gfloat          t_margin,
						    gfloat          b_margin);

G_GNUC_INTERNAL
void         eog_print_preview_set_from_page_setup (EogPrintPreview *preview,
						    GtkPageSetup    *setup);

G_GNUC_INTERNAL
void         eog_print_preview_get_image_position  (EogPrintPreview *preview,
						    gdouble         *x,
						    gdouble         *y);

G_GNUC_INTERNAL
void         eog_print_preview_set_image_position  (EogPrintPreview *preview,
						    gdouble          x,
						    gdouble          y);

G_GNUC_INTERNAL
gboolean     eog_print_preview_point_in_image_area (EogPrintPreview *preview,
						    guint            x,
						    guint            y);
G_GNUC_INTERNAL
void         eog_print_preview_set_scale           (EogPrintPreview *preview,
						    gfloat           scale);
G_GNUC_INTERNAL
gfloat       eog_print_preview_get_scale           (EogPrintPreview *preview);

G_END_DECLS
