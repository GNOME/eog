/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * preferences.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2001, SuSE GmbH.
 */

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <eog-image-view.h>

G_BEGIN_DECLS
 
GtkWidget *
eog_create_preferences_page (EogImageView *image_view,
			     guint page_number);

G_END_DECLS

#endif

