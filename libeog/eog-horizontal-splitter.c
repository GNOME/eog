/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eog-horizontal-splitter.c - A horizontal splitter with a semi gradient look

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
            Jens Finke <jens@triq.net>
*/

#include <config.h>
#include "eog-horizontal-splitter.h"
#include <libgnome/gnome-macros.h>
#include <stdlib.h>

struct _EogHorizontalSplitterPrivate {
	double press_x;
	guint32 press_time;
	int press_position;
	int saved_size;
};

#define CLOSED_THRESHOLD  8
#define NOMINAL_SIZE 148
#define SPLITTER_CLICK_SLOP 1
#define SPLITTER_CLICK_TIMEOUT	400

static void eog_horizontal_splitter_class_init (EogHorizontalSplitterClass *horizontal_splitter_class);
static void eog_horizontal_splitter_instance_init       (EogHorizontalSplitter      *horizontal_splitter);

GNOME_CLASS_BOILERPLATE (EogHorizontalSplitter,
			 eog_horizontal_splitter,
			 GtkHPaned, GTK_TYPE_HPANED)


static void
eog_horizontal_splitter_instance_init (EogHorizontalSplitter *horizontal_splitter)
{
	horizontal_splitter->private = g_new0 (EogHorizontalSplitterPrivate, 1);
}

static void
eog_horizontal_splitter_finalize (GObject *object)
{
	EogHorizontalSplitter *horizontal_splitter;
	
	horizontal_splitter = EOG_HORIZONTAL_SPLITTER (object);

	g_free (horizontal_splitter->private);
	
	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static int
absolute_position (EogHorizontalSplitter *splitter, int rel_position)
{
	gint widget_width;
	gint handle_width;

	widget_width = GTK_WIDGET (splitter)->allocation.width;
	gdk_drawable_get_size (GTK_PANED (splitter)->handle, &handle_width, NULL);

	return (widget_width - rel_position - handle_width);
}

static void
splitter_expand (EogHorizontalSplitter *splitter, int position)
{
	g_return_if_fail (EOG_IS_HORIZONTAL_SPLITTER (splitter));

	if (position <= absolute_position (splitter, CLOSED_THRESHOLD)) {
		return;
	}

	position = splitter->private->saved_size;
	if (position > absolute_position (splitter, CLOSED_THRESHOLD)) {
		position = absolute_position (splitter, NOMINAL_SIZE);
	}
	
	gtk_paned_set_position (GTK_PANED (splitter), position);
}

static void
splitter_collapse (EogHorizontalSplitter *splitter, int position)
{
	gint width;
	
	g_return_if_fail (EOG_IS_HORIZONTAL_SPLITTER (splitter));

	width = GTK_WIDGET (splitter)->allocation.width;

	splitter->private->saved_size = position;
	gtk_paned_set_position (GTK_PANED (splitter), width);
}

static void
splitter_toggle (EogHorizontalSplitter *splitter, int position)
{
	g_return_if_fail (EOG_IS_HORIZONTAL_SPLITTER (splitter));

	if (gtk_paned_get_position (GTK_PANED (splitter)) < absolute_position (splitter, CLOSED_THRESHOLD)) {
		eog_horizontal_splitter_collapse (splitter);
	} else {
		eog_horizontal_splitter_expand (splitter);
	}
}

static void
splitter_hide (EogHorizontalSplitter *splitter)
{
	GtkPaned *parent;

	parent = GTK_PANED (splitter);

	gtk_widget_hide (parent->child1);
}

static void
splitter_show (EogHorizontalSplitter *splitter)
{
	GtkPaned *parent;

	parent = GTK_PANED (splitter);

	gtk_widget_show (parent->child1);
}

static gboolean
splitter_is_hidden (EogHorizontalSplitter *splitter)
{
	GtkPaned *parent;
	
	parent = GTK_PANED (splitter);

	return GTK_WIDGET_VISIBLE (parent->child1);
}

void
eog_horizontal_splitter_expand (EogHorizontalSplitter *splitter)
{
	splitter_expand (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

void
eog_horizontal_splitter_hide (EogHorizontalSplitter *splitter)
{
	splitter_hide (splitter);
}

void
eog_horizontal_splitter_show (EogHorizontalSplitter *splitter)
{
	splitter_show (splitter);
}

gboolean
eog_horizontal_splitter_is_hidden (EogHorizontalSplitter *splitter)
{
	return splitter_is_hidden (splitter);
}

void
eog_horizontal_splitter_collapse (EogHorizontalSplitter *splitter)
{
	splitter_collapse (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

/* routine to toggle the open/closed state of the splitter */
void
eog_horizontal_splitter_toggle_position (EogHorizontalSplitter *splitter)
{
	splitter_toggle (splitter, gtk_paned_get_position (GTK_PANED (splitter)));
}

/* EogHorizontalSplitter public methods */
GtkWidget *
eog_horizontal_splitter_new (void)
{
	return gtk_widget_new (eog_horizontal_splitter_get_type (), NULL);
}

/* handle mouse downs by remembering the position and the time */
static gboolean
eog_horizontal_splitter_button_press (GtkWidget *widget, GdkEventButton *event)
{
	gboolean result;
	EogHorizontalSplitter *splitter;
	int position;
	
	splitter = EOG_HORIZONTAL_SPLITTER (widget);

	position = gtk_paned_get_position (GTK_PANED (widget));

	result = GNOME_CALL_PARENT_WITH_DEFAULT
		(GTK_WIDGET_CLASS, button_press_event, (widget, event), TRUE);

	if (result) {
		splitter->private->press_x = event->x;
		splitter->private->press_time = event->time;
		splitter->private->press_position = position;
	}

	return result;
}

/* handle mouse ups by seeing if it was a tap and toggling the open state accordingly */
static gboolean
eog_horizontal_splitter_button_release (GtkWidget *widget, GdkEventButton *event)
{
	gboolean result;
	EogHorizontalSplitter *splitter;
	int delta, delta_time;
	splitter = EOG_HORIZONTAL_SPLITTER (widget);

	result = GNOME_CALL_PARENT_WITH_DEFAULT
		(GTK_WIDGET_CLASS, button_release_event, (widget, event), TRUE);

	if (result) {
		delta = abs (event->x - splitter->private->press_x);
		delta_time = event->time - splitter->private->press_time;
		if (delta < SPLITTER_CLICK_SLOP && delta_time < SPLITTER_CLICK_TIMEOUT)  {
			eog_horizontal_splitter_toggle_position (splitter);
		}
	}

	return result;
}

static void
eog_horizontal_splitter_size_allocate (GtkWidget     *widget,
					    GtkAllocation *allocation)
{
	gint border_width;
	GtkPaned *paned;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
      
	paned = GTK_PANED (widget);
	border_width = GTK_CONTAINER (paned)->border_width;

	widget->allocation = *allocation;

	if (paned->child2 != NULL && GTK_WIDGET_VISIBLE (paned->child2)) { 
		GNOME_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate,
				 (widget, allocation));
	} else if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1)) {

		if (GTK_WIDGET_REALIZED (widget)) {
			gdk_window_hide (paned->handle);
		}

		gtk_widget_get_child_requisition (paned->child1,
						  &child_requisition);
		
		child_allocation.x = widget->allocation.x + border_width;
		child_allocation.y = widget->allocation.y + border_width;
		child_allocation.width = MIN (child_requisition.width,
					      allocation->width - 2 * border_width);
		child_allocation.height = MIN (child_requisition.height,
					       allocation->height - 2 * border_width);
		
		gtk_widget_size_allocate (paned->child1, &child_allocation);
	} else
		if (GTK_WIDGET_REALIZED (widget)) {
			gdk_window_hide (paned->handle);
		}

}

static void
eog_horizontal_splitter_class_init (EogHorizontalSplitterClass *class)
{
	GtkWidgetClass *widget_class;
	
	widget_class = GTK_WIDGET_CLASS (class);

	G_OBJECT_CLASS (class)->finalize = eog_horizontal_splitter_finalize;

	widget_class->size_allocate = eog_horizontal_splitter_size_allocate;
	widget_class->button_press_event = eog_horizontal_splitter_button_press;
	widget_class->button_release_event = eog_horizontal_splitter_button_release;
}

void
eog_horizontal_splitter_pack2 (EogHorizontalSplitter *splitter,
				    GtkWidget                  *child2)
{
	GtkPaned *paned;
	
	g_return_if_fail (GTK_IS_WIDGET (child2));
	g_return_if_fail (EOG_IS_HORIZONTAL_SPLITTER (splitter));

	paned = GTK_PANED (splitter);
	gtk_paned_pack2 (paned, child2, TRUE, TRUE);
}
