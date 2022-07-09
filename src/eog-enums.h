/* Eye of GNOME - General enumerations.
 *
 * Copyright (C) 2007-2008 The Free Software Foundation
 *
 * Author: Lucas Rocha <lucasr@gnome.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

typedef enum {
	EOG_IMAGE_DATA_IMAGE     = 1 << 0,
	EOG_IMAGE_DATA_DIMENSION = 1 << 1,
	EOG_IMAGE_DATA_EXIF      = 1 << 2,
	EOG_IMAGE_DATA_XMP       = 1 << 3
} EogImageData;

#define EOG_IMAGE_DATA_ALL  (EOG_IMAGE_DATA_IMAGE |     \
			     EOG_IMAGE_DATA_DIMENSION | \
			     EOG_IMAGE_DATA_EXIF |      \
			     EOG_IMAGE_DATA_XMP)

