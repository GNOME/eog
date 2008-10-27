/* Eye Of Gnome - Message Area
 *
 * Copyright (C) 2007 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
 *
 * Based on gedit code (gedit/gedit-message-area.h) by:
 * 	- Paolo Maggi <paolo@gnome.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EOG_MESSAGE_AREA_H__
#define __EOG_MESSAGE_AREA_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EogMessageArea EogMessageArea;
typedef struct _EogMessageAreaClass EogMessageAreaClass;
typedef struct _EogMessageAreaPrivate EogMessageAreaPrivate;

#define EOG_TYPE_MESSAGE_AREA            (eog_message_area_get_type())
#define EOG_MESSAGE_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EOG_TYPE_MESSAGE_AREA, EogMessageArea))
#define EOG_MESSAGE_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EOG_TYPE_MESSAGE_AREA, EogMessageAreaClass))
#define EOG_IS_MESSAGE_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EOG_TYPE_MESSAGE_AREA))
#define EOG_IS_MESSAGE_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_MESSAGE_AREA))
#define EOG_MESSAGE_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EOG_TYPE_MESSAGE_AREA, EogMessageAreaClass))

struct _EogMessageArea {
	GtkHBox parent;

	EogMessageAreaPrivate *priv;
};

struct _EogMessageAreaClass {
	GtkHBoxClass parent_class;

	void (* response) (EogMessageArea *message_area,
			   gint            response_id);

	void (* close)    (EogMessageArea *message_area);
};

GType 		 eog_message_area_get_type 		(void) G_GNUC_CONST;

GtkWidget	*eog_message_area_new      		(void);

GtkWidget	*eog_message_area_new_with_buttons	(const gchar      *first_button_text,
                                        		 ...);

void		 eog_message_area_set_contents	        (EogMessageArea *message_area,
                                             		 GtkWidget        *contents);

void		 eog_message_area_add_action_widget	(EogMessageArea   *message_area,
                                         		 GtkWidget        *child,
                                         		 gint              response_id);

GtkWidget	*eog_message_area_add_button        	(EogMessageArea *message_area,
                                         		 const gchar      *button_text,
                                         		 gint              response_id);

GtkWidget	*eog_message_area_add_stock_button_with_text
							(EogMessageArea   *message_area,
				    			 const gchar      *text,
				    			 const gchar      *stock_id,
				    			 gint              response_id);

void       	 eog_message_area_add_buttons 	        (EogMessageArea *message_area,
                                         		 const gchar      *first_button_text,
                                         		 ...);

void		 eog_message_area_set_response_sensitive
							(EogMessageArea   *message_area,
                                        		 gint              response_id,
                                        		 gboolean          setting);
void 		 eog_message_area_set_default_response
							(EogMessageArea *message_area,
                                        		 gint              response_id);

void		 eog_message_area_response           	(EogMessageArea *message_area,
                                    			 gint              response_id);

G_END_DECLS

#endif  /* __EOG_MESSAGE_AREA_H__ */
