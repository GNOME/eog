/*
 * Copyright 2002 Sun Microsystems Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __ACCESSIBLE_IMAGE_VIEW_H__
#define __ACCESSIBLE_IMAGE_VIEW_H__

#include <gtk/gtkaccessible.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ACCESSIBLE_TYPE_IMAGE_VIEW                     (accessible_image_view_get_type ())

#define ACCESSIBLE_IMAGE_VIEW(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCESSIBLE_TYPE_IMAGE_VIEW, AccessibleImageView))

#define ACCESSIBLE_IMAGE_VIEW_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), ACCESSIBLE_TYPE_IMAGE_VIEW, AccessibleImageViewClass))

#define ACCESSIBLE_IS_IMAGE_VIEW(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACCESSIBLE_TYPE_IMAGE_VIEW))

#define ACCESSIBLE_IS_IMAGE_VIEW_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), ACCESSIBLE_TYPE_IMAGE_VIEW))

#define ACCESSIBLE_IMAGE_VIEW_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), ACCESSIBLE_TYPE_IMAGE_VIEW, AccessibleImageViewClass))

typedef struct _AccessibleImageView                   AccessibleImageView;
typedef struct _AccessibleImageViewClass              AccessibleImageViewClass;

struct _AccessibleImageView
{
        GtkAccessible   parent;
	gchar*     image_description;
};

struct _AccessibleImageViewClass
{
        GtkAccessibleClass parent_class;
};

GType accessible_image_view_get_type (void);

AtkObject* accessible_image_view_new (GtkWidget *widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ACCESSIBLE_IMAGE_VIEW_H__ */
