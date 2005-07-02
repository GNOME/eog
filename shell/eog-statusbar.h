/*
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

#ifndef EOG_STATUSBAR_H
#define EOG_STATUSBAR_H

#include <gtk/gtkstatusbar.h>

G_BEGIN_DECLS

#define EOG_TYPE_STATUSBAR		(eog_statusbar_get_type ())
#define EOG_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_STATUSBAR, EogStatusbar))
#define EOG_STATUSBAR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_STATUSBAR, EogStatusbarClass))
#define EOG_IS_STATUSBAR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_STATUSBAR))
#define EOG_IS_STATUSBAR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_STATUSBAR))
#define EOG_STATUSBAR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_STATUSBAR, EogStatusbarClass))

typedef struct _EogStatusbar		EogStatusbar;
typedef struct _EogStatusbarPrivate	EogStatusbarPrivate;
typedef struct _EogStatusbarClass	EogStatusbarClass;

struct _EogStatusbar
{
        GtkStatusbar parent;

        EogStatusbarPrivate *priv;
};

struct _EogStatusbarClass
{
        GtkStatusbarClass parent_class;
};

GType		 eog_statusbar_get_type			(void) G_GNUC_CONST;

GtkWidget	*eog_statusbar_new			(void);

void		 eog_statusbar_set_has_resize_grip	(EogStatusbar   *statusbar,
							 gboolean          show);

void		 eog_statusbar_set_image_number		(EogStatusbar   *statusbar,
							 gint            num,
							 gint            tot);

void		 eog_statusbar_set_progress		(EogStatusbar   *statusbar,
							 gdouble         progress);

G_END_DECLS

#endif /* EOG_STATUSBAR_H */
