/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-window.h.
 *
 * Authors:
 *   Federico Mena-Quintero (federico@gnu.org)
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright (C) 2000-2001 The Free Software Foundation.
 * Copyright (C) 2001 SuSE GmbH.
 */

#ifndef _EOG_WINDOW_H_
#define _EOG_WINDOW_H_

#include <bonobo.h>

BEGIN_GNOME_DECLS
 
#define EOG_WINDOW_TYPE           (eog_window_get_type ())
#define EOG_WINDOW(o)             (GTK_CHECK_CAST ((o), EOG_WINDOW_TYPE, EogWindow))
#define EOG_WINDOW_CLASS(k)       (GTK_CHECK_CLASS_CAST((k), EOG_WINDOW_TYPE, EogWindowClass))

#define EOG_IS_WINDOW(o)          (GTK_CHECK_TYPE ((o), EOG_WINDOW_TYPE))
#define EOG_IS_WINDOW_CLASS(k)    (GTK_CHECK_CLASS_TYPE ((k), EOG_WINDOW_TYPE))

typedef struct _EogWindow         EogWindow;
typedef struct _EogWindowClass    EogWindowClass;
typedef struct _EogWindowPrivate  EogWindowPrivate;

struct _EogWindow {
	BonoboWindow window;

	EogWindowPrivate *priv;
};

struct _EogWindowClass {
	BonoboWindowClass parent_class;
};

GtkType        eog_window_get_type                    (void);
EogWindow     *eog_window_new                         (const char   *title);
EogWindow     *eog_window_construct                   (EogWindow    *window,
						       const char   *title);

void           eog_window_launch_component            (EogWindow    *window,
						       const char   *moniker);

END_GNOME_DECLS

#endif /* _EOG_WINDOW_H_ */

