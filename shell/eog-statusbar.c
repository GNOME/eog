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


#include <config.h>

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkwidget.h>

#include "eog-statusbar.h"

#define EOG_STATUSBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_STATUSBAR, EogStatusbarPrivate))

struct _EogStatusbarPrivate
{
	GtkWidget *progressbar;
	GtkWidget *img_info_statusbar;
	GtkWidget *img_num_statusbar;
};

G_DEFINE_TYPE(EogStatusbar, eog_statusbar, GTK_TYPE_STATUSBAR)

static void
eog_statusbar_notify (GObject    *object,
		      GParamSpec *pspec)
{
	/* don't allow gtk_statusbar_set_has_resize_grip to mess with us.
	 * See eog_statusbar_set_has_resize_grip for an explanation.
	 */
	if (strcmp (g_param_spec_get_name (pspec), "has-resize-grip") == 0)
	{
		gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (object), FALSE);
		return;
	}

	if (G_OBJECT_CLASS (eog_statusbar_parent_class)->notify)
		G_OBJECT_CLASS (eog_statusbar_parent_class)->notify (object, pspec);
}

static void
eog_statusbar_class_init (EogStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->notify = eog_statusbar_notify;

	g_type_class_add_private (object_class, sizeof (EogStatusbarPrivate));
}

static void
eog_statusbar_init (EogStatusbar *statusbar)
{
	GtkRequisition requisition;

	statusbar->priv = EOG_STATUSBAR_GET_PRIVATE (statusbar);

	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (statusbar), FALSE);

	statusbar->priv->img_num_statusbar = gtk_statusbar_new ();
	gtk_widget_show (statusbar->priv->img_num_statusbar);

	/* reasonable fixed width for "i / n" */
	gtk_widget_set_size_request (statusbar->priv->img_num_statusbar, 80, 10);
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (statusbar->priv->img_num_statusbar),
					   TRUE);
	gtk_box_pack_end (GTK_BOX (statusbar),
			  statusbar->priv->img_num_statusbar,
			  FALSE, TRUE, 0);

	statusbar->priv->progressbar = gtk_progress_bar_new ();
	gtk_box_pack_end (GTK_BOX (statusbar),
			  statusbar->priv->progressbar,
			  FALSE, TRUE, 0);
	gtk_widget_hide (statusbar->priv->progressbar);

	gtk_widget_set_size_request (statusbar->priv->progressbar, 
				     -1, 10);
}

GtkWidget *
eog_statusbar_new (void)
{
	return GTK_WIDGET (g_object_new (EOG_TYPE_STATUSBAR, NULL));
}

 /*
  * I don't like this much, in a perfect world it would have been
  * possible to override the parent property and use
  * gtk_statusbar_set_has_resize_grip. Unfortunately this is not
  * possible and it's not even possible to intercept the notify signal
  * since the parent property should always be set to false thus when
  * using set_resize_grip (FALSE) the property doesn't change and the 
  * notification is not emitted.
  */
void
eog_statusbar_set_has_resize_grip (EogStatusbar *bar,
				   gboolean      show)
{
	g_return_if_fail (EOG_IS_STATUSBAR (bar));

	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (bar->priv->img_num_statusbar),
					   show);
}

void
eog_statusbar_set_image_number (EogStatusbar *statusbar,
                                gint          num,
				gint          tot)
{
	gchar *msg;

	g_return_if_fail (EOG_IS_STATUSBAR (statusbar));

	gtk_statusbar_pop (GTK_STATUSBAR (statusbar->priv->img_num_statusbar), 0); 
	msg = g_strdup_printf ("%d / %d", num, tot);
	gtk_statusbar_push (GTK_STATUSBAR (statusbar->priv->img_num_statusbar), 0, msg);

      	g_free (msg);
}

void
eog_statusbar_set_progress (EogStatusbar *statusbar,
			    gdouble       progress)
{
	g_return_if_fail (EOG_IS_STATUSBAR (statusbar));

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (statusbar->priv->progressbar),
				       progress);

	if (progress < 1)
		gtk_widget_show (statusbar->priv->progressbar);
	else
		gtk_widget_hide (statusbar->priv->progressbar);
}
