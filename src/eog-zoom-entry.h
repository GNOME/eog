/*
 * eog-zoom-entry.h
 * This file is part of eog
 *
 * Author: Felix Riemann <friemann@gnome.org>
 *
 * Copyright (C) 2017 GNOME Foundation
 *
 * Based on code (ev-zoom-action.c) by:
 *      - Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "eog-scroll-view.h"

#define EOG_TYPE_ZOOM_ENTRY (eog_zoom_entry_get_type())

G_DECLARE_FINAL_TYPE(EogZoomEntry, eog_zoom_entry, EOG, ZOOM_ENTRY, GtkBox);

GtkWidget* eog_zoom_entry_new (EogScrollView *view, GMenu *menu);
