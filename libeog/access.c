/* Eye of Gnome image viewer - Utility functions for accessibility 
 *
 * Copyright (C) 2002 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gnu.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "access.h"



void
access_add_atk_relation (GtkWidget *widget1, GtkWidget *widget2,
			 AtkRelationType w1_to_w2, AtkRelationType w2_to_w1)
{
	AtkObject      *atk_widget1;
	AtkObject      *atk_widget2;
	AtkRelationSet *relation_set;
	AtkRelation    *relation;
	AtkObject      *targets[1];

	g_return_if_fail (GTK_IS_WIDGET (widget1));
	g_return_if_fail (GTK_IS_WIDGET (widget2));

	atk_widget1 = gtk_widget_get_accessible (widget1);
	atk_widget2 = gtk_widget_get_accessible (widget2);

	/* Create the widget1 -> widget2 relation */
	relation_set = atk_object_ref_relation_set (atk_widget1);
	targets[0] = atk_widget2;
	relation = atk_relation_new (targets, 1, w1_to_w2);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (relation);

	/* Create the widget2 -> widget1 relation */
	relation_set = atk_object_ref_relation_set (atk_widget2);
	targets[0] = atk_widget1;
	relation = atk_relation_new (targets, 1, w2_to_w1);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (relation);
}
