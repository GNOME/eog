/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * eog-util.h.
 *
 * Authors:
 *   Martin Baulig (baulig@suse.de)
 *
 * Copyright 2000, SuSE GmbH.
 */

#ifndef _EOG_UTIL_H_
#define _EOG_UTIL_H_

#include <Eog.h>
#include <bonobo.h>

BEGIN_GNOME_DECLS

void
eog_util_add_interfaces                 (BonoboObject            *object,
                                         BonoboObject            *query_this,
                                         const gchar            **interfaces);

GSList *
eog_util_split_string                   (const gchar             *string,
                                         const gchar             *delimiter);

END_GNOME_DECLS

#endif /* _EOG_UTIL_H_ */
