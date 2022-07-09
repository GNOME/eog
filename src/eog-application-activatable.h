/*
 * eog-application-activatable.h
 * This file is part of eog
 *
 * Author: Felix Riemann <friemann@gnome.org>
 *
 * Copyright (C) 2012 Felix Riemann
 * 
 * Base on code by:
 * 	- Steve Frécinaux <code@istique.net>
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

#include <glib-object.h>

G_BEGIN_DECLS

#define EOG_TYPE_APPLICATION_ACTIVATABLE (eog_application_activatable_get_type ())
G_DECLARE_INTERFACE (EogApplicationActivatable, eog_application_activatable, EOG, APPLICATION_ACTIVATABLE, GObject);

struct _EogApplicationActivatableInterface
{
	GTypeInterface g_iface;

	/* vfuncs */

    void	(*activate)	(EogApplicationActivatable *activatable);
    void	(*deactivate)	(EogApplicationActivatable *activatable);
};

void	eog_application_activatable_activate     (EogApplicationActivatable *activatable);
void	eog_application_activatable_deactivate   (EogApplicationActivatable *activatable);

G_END_DECLS
