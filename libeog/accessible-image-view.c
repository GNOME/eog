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

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkwidget.h>
#include <sys/types.h>
#include <bonobo.h>

#include "accessible-image-view.h"
#include "image-view.h"

static void accessible_image_view_class_init       (AccessibleImageViewClass *klass);

static void accessible_image_view_object_init	   (AccessibleImageView *image);

static gint accessible_image_view_get_n_children   (AtkObject       *obj);


static void accessible_image_view_finalize         (GObject        *object);


/* AtkImage Interfaces */
static void  atk_image_interface_init   (AtkImageIface  *iface);
static G_CONST_RETURN gchar* accessible_image_view_get_image_description
                                                        (AtkImage       *obj);

static gboolean accessible_image_view_set_image_description (AtkImage       *obj,
                                                         const gchar    *description);

static void accessible_image_view_get_image_size (AtkImage *obj,
                                             gint     *width,
                                             gint     *height);

static gpointer parent_class = NULL;

GType
accessible_image_view_get_type (void)
{
        static GType type = 0;

        if (!type)
        {
                static GTypeInfo tinfo =
                {
                        0,
                        (GBaseInitFunc) NULL, /* base init */
                        (GBaseFinalizeFunc) NULL, /* base finalize */
                        (GClassInitFunc) accessible_image_view_class_init, /* class init */
                        (GClassFinalizeFunc) NULL, /* class finalize */
                        NULL, /* class data */
                        0, /* instance size */
                        0, /* nb preallocs */
                        (GInstanceInitFunc) accessible_image_view_object_init, /* instance init */
                        NULL /* value table */
                };

	        static const GInterfaceInfo atk_image_info =
    		{
        		(GInterfaceInitFunc) atk_image_interface_init,
        		(GInterfaceFinalizeFunc) NULL,
        		NULL
    		};

                /*
                 * Figure out the size of the class and instance
                 * we are deriving from
                 */
                AtkObjectFactory *factory;
                GType derived_type;
                GTypeQuery query;
                GType derived_atk_type;

                derived_type = g_type_parent (TYPE_IMAGE_VIEW);
                factory = atk_registry_get_factory (atk_get_default_registry(),
                                                    derived_type);
                derived_atk_type = atk_object_factory_get_accessible_type (factory);
                g_type_query (derived_atk_type, &query);
                tinfo.class_size = query.class_size;
                tinfo.instance_size = query.instance_size;

                type = g_type_register_static(derived_atk_type,
                                              "AccessibleImageView", &tinfo, 0);
	    
	        g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                 &atk_image_info);

        }

        return type;
}

static void
accessible_image_view_class_init (AccessibleImageViewClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
        
	g_return_if_fail(class != NULL);
	parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = accessible_image_view_finalize;
        class->get_n_children = accessible_image_view_get_n_children;
}

static void
accessible_image_view_object_init (AccessibleImageView *image)
{
  image->image_description = NULL;
}


AtkObject *
accessible_image_view_new (GtkWidget *widget)
{
        GObject *object;
        AtkObject *accessible;
	GtkAccessible *gtk_accessible;

        object = g_object_new (ACCESSIBLE_TYPE_IMAGE_VIEW, NULL);
        g_return_val_if_fail(object != NULL, NULL);

        accessible = ATK_OBJECT (object);
	gtk_accessible = GTK_ACCESSIBLE (accessible);
	gtk_accessible->widget = widget;

	atk_object_initialize (accessible, widget);

        accessible->role = ATK_ROLE_IMAGE;

        return accessible;
}

static gint
accessible_image_view_get_n_children (AtkObject* obj)
{
        return 0;
}

static void
accessible_image_view_finalize (GObject *object)
{
        AccessibleImageView *imageview = ACCESSIBLE_IMAGE_VIEW(object);
	
	g_free (imageview->image_description);
        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_image_description = accessible_image_view_get_image_description;
  iface->set_image_description = accessible_image_view_set_image_description;
  iface->get_image_size = accessible_image_view_get_image_size;
}

static G_CONST_RETURN gchar*
accessible_image_view_get_image_description (AtkImage *obj) {

 AccessibleImageView *image;
 
 g_return_val_if_fail(ACCESSIBLE_IS_IMAGE_VIEW(obj), NULL);
 
 image = ACCESSIBLE_IMAGE_VIEW (obj);

  
 return image->image_description;
}


static gboolean
accessible_image_view_set_image_description (AtkImage *obj,
                                             const gchar *description)
{
  AccessibleImageView *image;
  
  image = ACCESSIBLE_IMAGE_VIEW (obj);
 
  if (image->image_description) 
  	g_free (image->image_description);
  
  image->image_description = g_strdup (description);

  return TRUE;

}

static void
accessible_image_view_get_image_size (AtkImage *obj,
                            gint     *width,
                            gint     *height)
{
  GtkWidget *widget;
  GdkPixbuf *image;

  widget = GTK_ACCESSIBLE (obj)->widget;

  if (widget == 0)
  {
    /*
     * State is defunct
     */
    *width = -1;
    *height = -1;
    return;
  }

  image = image_view_get_pixbuf (IMAGE_VIEW (widget));

  *height = gdk_pixbuf_get_height(image);
  *width = gdk_pixbuf_get_width(image);

}


