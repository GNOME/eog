/* Eye of Gnome image viewer - main eog_window widget
 *
 * Copyright (C) 2000 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
 *         Jens Finke <jens@gnome.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EOG_WINDOW_H
#define EOG_WINDOW_H

#include <bonobo.h>

BEGIN_GNOME_DECLS
 


#define TYPE_EOG_WINDOW            (eog_window_get_type ())
#define EOG_WINDOW(obj)            (GTK_CHECK_CAST ((obj), TYPE_EOG_WINDOW, EogWindow))
#define EOG_WINDOW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_EOG_WINDOW, EogWindowClass))
#define EOG_IS_WINDOW(obj)         (GTK_CHECK_TYPE ((obj), TYPE_EOG_WINDOW))
#define EOG_IS_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_EOG_WINDOW))


typedef struct _EogWindow         EogWindow;
typedef struct _EogWindowClass    EogWindowClass;

typedef struct _EogWindowPrivate  EogWindowPrivate;


struct _EogWindow {
	BonoboWindow win;

	/* Private data */
	EogWindowPrivate *priv;
};

struct _EogWindowClass {
	BonoboWindowClass parent_class;
};


GtkType        eog_window_get_type                    (void);
GtkWidget *eog_window_new (void);
void eog_window_construct (EogWindow *eog_window);

void eog_window_close (EogWindow *eog_window);

void eog_window_open_dialog (EogWindow *eog_window);
gboolean eog_window_open (EogWindow *eog_window, const char *path);

void eog_window_set_auto_size (EogWindow *eog_window, gboolean bool);
gboolean eog_window_get_auto_size (EogWindow *eog_window);

Bonobo_PropertyControl eog_window_get_property_control (EogWindow *eog_window,
							CORBA_Environment *ev);

Bonobo_UIContainer eog_window_get_ui_container (EogWindow *eog_window,
						CORBA_Environment *ev);

END_GNOME_DECLS

#endif
