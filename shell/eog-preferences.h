/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-preferences.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_PREFERENCES_H_
#define _EOG_PREFERENCES_H_

#include <eog-window.h>

BEGIN_GNOME_DECLS
 
#define EOG_PREFERENCES_TYPE           (eog_preferences_get_type ())
#define EOG_PREFERENCES(o)             (GTK_CHECK_CAST ((o), EOG_PREFERENCES_TYPE, EogPreferences))
#define EOG_PREFERENCES_CLASS(k)       (GTK_CHECK_CLASS_CAST((k), EOG_PREFERENCES_TYPE, EogPreferencesClass))

#define EOG_IS_PREFERENCES(o)          (GTK_CHECK_TYPE ((o), EOG_PREFERENCES_TYPE))
#define EOG_IS_PREFERENCES_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), EOG_PREFERENCES_TYPE))

typedef struct _EogPreferences         EogPreferences;
typedef struct _EogPreferencesClass    EogPreferencesClass;
typedef struct _EogPreferencesPrivate  EogPreferencesPrivate;

struct _EogPreferences {
	GnomePropertyBox dialog;

	EogPreferencesPrivate *priv;
};

struct _EogPreferencesClass {
	GnomePropertyBoxClass parent_class;
};

GtkType         eog_preferences_get_type     (void);
EogPreferences *eog_preferences_new          (EogWindow      *window);
EogPreferences *eog_preferences_construct    (EogPreferences *preferences,
					      EogWindow      *window);

END_GNOME_DECLS

#endif _EOG_EOG_PREFERENCES
