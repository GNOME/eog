/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  GNOME::EOG::CollectionView - C-implementation of the bonobo
 *  interface.
 *
 *  Authors: Jens Finke <jens@triq.net>
 *
 *  Copyright (C) 2004 Jens Finke
 */
#include <config.h>
#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include "eog-collection-view-iface.h"

static GObjectClass *parent_class = NULL;

enum {
	LOAD_URI_LIST,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

#define CLASS(o) BONOBO_COLLECTION_VIEW_IFACE_GET_CLASS(o)

static void
impl_GNOME_EOG_CollectionView__load_uri_list (PortableServer_Servant  servant,
					      const GNOME_EOG_URIList *uri_list,
					      CORBA_Environment       *ev)
{
	GList *list = NULL;
	int i;
	EogCollectionViewIface *iface;

	iface = EOG_COLLECTION_VIEW_IFACE (bonobo_object_from_servant (servant));

        for (i = 0; i < uri_list->_length; i++) {
                list = g_list_prepend (list, g_strdup (uri_list->_buffer[i]));
        }

	list = g_list_reverse (list);

	g_signal_emit (G_OBJECT (iface), signals [LOAD_URI_LIST], 0, list);
}

static void
eog_collection_view_iface_class_init (EogCollectionViewIfaceClass *klass)
{
	POA_GNOME_EOG_CollectionView__epv *epv = &klass->epv;
	GObjectClass *object_class;
	
	object_class = (GObjectClass *) klass;
	
	parent_class = g_type_class_peek_parent (klass);

	signals [LOAD_URI_LIST] =
		g_signal_new ("load_uri_list",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogCollectionViewIfaceClass, load_uri_list),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	epv->loadURIList = impl_GNOME_EOG_CollectionView__load_uri_list;
}

static void
eog_collection_view_iface_init (EogCollectionViewIface *collection_view)
{
}

BONOBO_TYPE_FUNC_FULL (EogCollectionViewIface, GNOME_EOG_CollectionView, BONOBO_TYPE_OBJECT, eog_collection_view_iface)


/**
 * eog_collection_view_iface_new:
 * 
 * Create a new bonobo-collection_view implementing BonoboObject
 * interface.
 * 
 * Return value: 
 **/
EogCollectionViewIface *
eog_collection_view_iface_new (void)
{
	return g_object_new (eog_collection_view_iface_get_type (), NULL);
}
