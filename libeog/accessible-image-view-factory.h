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

#ifndef __ACCESSIBLE_IMAGE_VIEW_FACTORY_H__
#define __ACCESSIBLE_IMAGE_VIEW_FACTORY_H__

#include <atk/atkobjectfactory.h>
#include <image-view.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ACCESSIBLE_TYPE_IMAGE_VIEW_FACTORY         (accessible_image_view_factory_get_type())

#define ACCESSIBLE_IMAGE_VIEW_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACCESSIBLE_TYPE_IMAGE_VIEW_FACTORY, AccessibleImageViewFactory))

#define ACCESSIBLE_IMAGE_VIEW_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ACCESSIBLE_TYPE_IMAGE_VIEW_FACTORY, AccessibleImageViewFactoryClass))

#define ACCESSIBLE_IS_IMAGE_VIEW_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACCESSIBLE_TYPE_IMAGE_VIEW_FACTORY))

#define ACCESSIBLE_IS_IMAGE_VIEW_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ACCESSIBLE_TYPE_IMAGE_VIEW_FACTORY))

#define ACCESSIBLE_IMAGE_VIEW_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ACCESSIBLE_TYPE_IMAGE_VIEW_FACTORY, AccessibleImageViewFactoryClass))

typedef struct _AccessibleImageViewFactory       AccessibleImageViewFactory;
typedef struct _AccessibleImageViewFactoryClass  AccessibleImageViewFactoryClass;

struct _AccessibleImageViewFactory
{
        AtkObjectFactory parent;
};

struct _AccessibleImageViewFactoryClass
{
        AtkObjectFactoryClass parent_class;
};

GType accessible_image_view_get_type (void);
AtkObjectFactory *accessible_image_view_factory_new (void);

GType accessible_image_view_factory_get_type ();

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* __ACCESSIBLE_IMAGE_VIEW_FACTORY_H__ */
