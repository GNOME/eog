/* GNOME libraries - canvas item for icon captions
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gnu.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GNOME_ICON_CAPTION_H
#define GNOME_ICON_CAPTION_H

#include <libgnomeui/gnome-canvas.h>

G_BEGIN_DECLS



#define GNOME_TYPE_ICON_CAPTION            (gnome_icon_caption_get_type ())
#define GNOME_ICON_CAPTION(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_ICON_CAPTION,	\
					    GnomeIconCaption))
#define GNOME_ICON_CAPTION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),			\
					    GNOME_TYPE_ICON_CAPTION, GnomeIconCaptionClass))
#define GNOME_IS_ICON_CAPTION(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_ICON_CAPTION))
#define GNOME_IS_ICON_CAPTION_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_ICON_CAPTION))


typedef struct _GnomeIconCaption GnomeIconCaption;
typedef struct _GnomeIconCaptionClass GnomeIconCaptionClass;

struct _GnomeIconCaption {
	GnomeCanvasItem item;

	/* Private data */
	gpointer priv;
};

struct _GnomeIconCaptionClass {
	GnomeCanvasItemClass parent_class;

	/* Requesting signals */

	const char *(* request_text) (GnomeIconCaption *caption);
};


GtkType gnome_icon_caption_get_type (void);

void gnome_icon_caption_text_changed (GnomeIconCaption *caption);



G_END_DECLS

#endif
