/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eog-horizontal-splitter.h - A horizontal splitter with a semi gradient look

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

#ifndef EOG_HORIZONTAL_SPLITTER_H
#define EOG_HORIZONTAL_SPLITTER_H

#include <gtk/gtkhpaned.h>

G_BEGIN_DECLS

#define EOG_TYPE_HORIZONTAL_SPLITTER            (eog_horizontal_splitter_get_type ())
#define EOG_HORIZONTAL_SPLITTER(obj)            (GTK_CHECK_CAST ((obj), EOG_TYPE_HORIZONTAL_SPLITTER, EogHorizontalSplitter))
#define EOG_HORIZONTAL_SPLITTER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EOG_TYPE_HORIZONTAL_SPLITTER, EogHorizontalSplitterClass))
#define EOG_IS_HORIZONTAL_SPLITTER(obj)         (GTK_CHECK_TYPE ((obj), EOG_TYPE_HORIZONTAL_SPLITTER))
#define EOG_IS_HORIZONTAL_SPLITTER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EOG_TYPE_HORIZONTAL_SPLITTER))

typedef struct _EogHorizontalSplitterPrivate EogHorizontalSplitterPrivate;

typedef struct {
	GtkHPaned				parent_slot;
	EogHorizontalSplitterPrivate	*private;
} EogHorizontalSplitter;

typedef struct {
	GtkHPanedClass				parent_slot;
} EogHorizontalSplitterClass;

/* EogHorizontalSplitter public methods */
GType      eog_horizontal_splitter_get_type (void);
GtkWidget *eog_horizontal_splitter_new      (void);

gboolean   eog_horizontal_splitter_is_hidden	(EogHorizontalSplitter *splitter);
void	   eog_horizontal_splitter_collapse	(EogHorizontalSplitter *splitter);
void	   eog_horizontal_splitter_hide		(EogHorizontalSplitter *splitter);
void	   eog_horizontal_splitter_show		(EogHorizontalSplitter *splitter);
void	   eog_horizontal_splitter_expand		(EogHorizontalSplitter *splitter);
void	   eog_horizontal_splitter_toggle_position	(EogHorizontalSplitter *splitter);
void	   eog_horizontal_splitter_pack2           (EogHorizontalSplitter *splitter,
						    GtkWidget                  *child2);

G_END_DECLS

#endif /* EOG_HORIZONTAL_SPLITTER_H */
