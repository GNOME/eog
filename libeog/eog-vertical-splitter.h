/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eog-vertical-splitter.h - A vertical splitter with a semi gradient look

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
*/

#ifndef EOG_VERTICAL_SPLITTER_H
#define EOG_VERTICAL_SPLITTER_H

#include <gtk/gtkvpaned.h>

G_BEGIN_DECLS

#define EOG_TYPE_VERTICAL_SPLITTER            (eog_vertical_splitter_get_type ())
#define EOG_VERTICAL_SPLITTER(obj)            (GTK_CHECK_CAST ((obj), EOG_TYPE_VERTICAL_SPLITTER, EogVerticalSplitter))
#define EOG_VERTICAL_SPLITTER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_VERTICAL_SPLITTER, EogVerticalSplitterClass))
#define EOG_IS_VERTICAL_SPLITTER(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_VERTICAL_SPLITTER))
#define EOG_IS_VERTICAL_SPLITTER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_VERTICAL_SPLITTER))

typedef struct _EogVerticalSplitterPrivate EogVerticalSplitterPrivate;

typedef struct {
	GtkVPaned			parent_slot;
	EogVerticalSplitterPrivate	*private;
} EogVerticalSplitter;

typedef struct {
	GtkVPanedClass				parent_slot;
} EogVerticalSplitterClass;

/* EogVerticalSplitter public methods */
GType      eog_vertical_splitter_get_type (void);
GtkWidget *eog_vertical_splitter_new      (void);

gboolean   eog_vertical_splitter_is_hidden	(EogVerticalSplitter *splitter);
void	   eog_vertical_splitter_collapse	(EogVerticalSplitter *splitter);
void	   eog_vertical_splitter_hide		(EogVerticalSplitter *splitter);
void	   eog_vertical_splitter_show		(EogVerticalSplitter *splitter);
void	   eog_vertical_splitter_expand		(EogVerticalSplitter *splitter);
void	   eog_vertical_splitter_toggle_position	(EogVerticalSplitter *splitter);
void	   eog_vertical_splitter_pack2           (EogVerticalSplitter *splitter,
						    GtkWidget                  *child2);

G_END_DECLS

#endif /* EOG_VERTICAL_SPLITTER_H */
