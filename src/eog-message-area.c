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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-message-area.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#define EOG_MESSAGE_AREA_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EOG_TYPE_MESSAGE_AREA, EogMessageAreaPrivate))

G_DEFINE_TYPE(EogMessageArea, eog_message_area, GTK_TYPE_HBOX)

struct _EogMessageAreaPrivate
{
	GtkWidget *main_hbox;

	GtkWidget *contents;
	GtkWidget *action_area;

	gboolean changing_style;
};

typedef struct _ResponseData ResponseData;

struct _ResponseData
{
	gint response_id;
};

enum {
	SIGNAL_RESPONSE,
	SIGNAL_CLOSE,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
eog_message_area_finalize (GObject *object)
{
	G_OBJECT_CLASS (eog_message_area_parent_class)->finalize (object);
}

static ResponseData *
get_response_data (GtkWidget *widget,
		   gboolean   create)
{
	ResponseData *ad = g_object_get_data (G_OBJECT (widget),
                                       	      "eog-message-area-response-data");

	if (ad == NULL && create) {
		ad = g_new (ResponseData, 1);

		g_object_set_data_full (G_OBJECT (widget),
					"eog-message-area-response-data",
					ad,
					g_free);
    	}

	return ad;
}

static GtkWidget *
find_button (EogMessageArea *message_area,
	     gint              response_id)
{
	GList *children, *tmp_list;
	GtkWidget *child = NULL;

	children = gtk_container_get_children (
			GTK_CONTAINER (message_area->priv->action_area));

	for (tmp_list = children; tmp_list; tmp_list = tmp_list->next) {
		ResponseData *rd = get_response_data (tmp_list->data, FALSE);

		if (rd && rd->response_id == response_id) {
			child = tmp_list->data;
			break;
		}
	}

	g_list_free (children);

	return child;
}

static void
eog_message_area_close (EogMessageArea *message_area)
{
	if (!find_button (message_area, GTK_RESPONSE_CANCEL))
		return;

	/* Emit response signal */
	eog_message_area_response (EOG_MESSAGE_AREA (message_area),
				   GTK_RESPONSE_CANCEL);
}

static void
size_allocate (GtkWidget *widget,
	      GtkAllocation *allocation,
	      gpointer user_data)
{
	gtk_widget_queue_draw (widget);
}

static gboolean
paint_message_area (GtkWidget      *widget,
		    GdkEventExpose *event,
		    gpointer        user_data)
{
	gtk_paint_flat_box (widget->style,
			    widget->window,
			    GTK_STATE_NORMAL,
			    GTK_SHADOW_OUT,
			    NULL,
			    widget,
			    "tooltip",
			    widget->allocation.x + 1,
			    widget->allocation.y + 1,
			    widget->allocation.width - 2,
			    widget->allocation.height - 2);

	return FALSE;
}

static void
style_set (GtkWidget      *widget,
	   GtkStyle       *prev_style,
	   EogMessageArea *message_area)
{
        GtkWidget *window;
        GtkStyle *style;

        if (message_area->priv->changing_style)
                return;

        /* This is a hack needed to use the tooltip background color */
        window = gtk_window_new (GTK_WINDOW_POPUP);
        gtk_widget_set_name (window, "gtk-tooltip");
        gtk_widget_ensure_style (window);
        style = gtk_widget_get_style (window);

        message_area->priv->changing_style = TRUE;
        gtk_widget_set_style (GTK_WIDGET (message_area), style);
        message_area->priv->changing_style = FALSE;

        gtk_widget_destroy (window);

        gtk_widget_queue_draw (GTK_WIDGET (message_area));
}

static void
eog_message_area_class_init (EogMessageAreaClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet *binding_set;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	object_class->finalize = eog_message_area_finalize;

	class->close = eog_message_area_close;

	g_type_class_add_private (object_class, sizeof(EogMessageAreaPrivate));

/**
 * EogMessageArea::response:
 * @message_area: the object which received the signal.
 *
 * The #EogMessageArea::response signal is emitted when one of the
 * activatable widgets packed into @message_area is activated.
 */
	signals[SIGNAL_RESPONSE] =
		g_signal_new ("response",
			      G_OBJECT_CLASS_TYPE (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EogMessageAreaClass, response),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals[SIGNAL_CLOSE] =
		g_signal_new ("close",
			      G_OBJECT_CLASS_TYPE (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EogMessageAreaClass, close),
		  	      NULL, NULL,
		 	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);

	gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "close", 0);
}

static void
eog_message_area_init (EogMessageArea *message_area)
{
	message_area->priv = EOG_MESSAGE_AREA_GET_PRIVATE (message_area);

        /* FIXME: use style properties */
	message_area->priv->main_hbox = gtk_hbox_new (FALSE, 16);
	gtk_widget_show (message_area->priv->main_hbox);

	/* FIXME: use style properties */
	gtk_container_set_border_width (GTK_CONTAINER (message_area->priv->main_hbox),
					8);

        /* FIXME: use style properties */
	message_area->priv->action_area = gtk_vbox_new (TRUE, 10);
	gtk_widget_show (message_area->priv->action_area);

	gtk_box_pack_end (GTK_BOX (message_area->priv->main_hbox),
			    message_area->priv->action_area,
			    FALSE,
			    TRUE,
			    0);

	gtk_box_pack_start (GTK_BOX (message_area),
			    message_area->priv->main_hbox,
			    TRUE,
			    TRUE,
			    0);

	/* CHECK: do we really need it? */
	gtk_widget_set_name (GTK_WIDGET (message_area), "gtk-tooltips");

	g_signal_connect (message_area,
			  "expose_event",
			  G_CALLBACK (paint_message_area),
			  NULL);
	g_signal_connect (message_area,
			  "size-allocate",
			  G_CALLBACK (size_allocate),
			  NULL);


        /* Note that we connect to style-set on one of the internal
         * widgets, not on the message area itself, since gtk does
         * not deliver any further style-set signals for a widget on
         * which the style has been forced with gtk_widget_set_style() */
        g_signal_connect (message_area->priv->main_hbox,
                          "style-set",
                          G_CALLBACK (style_set),
                          message_area);
}

static gint
get_response_for_widget (EogMessageArea *message_area,
			 GtkWidget      *widget)
{
	ResponseData *rd;

	rd = get_response_data (widget, FALSE);
	if (!rd)
		return GTK_RESPONSE_NONE;
	else
		return rd->response_id;
}

static void
action_widget_activated (GtkWidget *widget, EogMessageArea *message_area)
{
	gint response_id;

	response_id = get_response_for_widget (message_area, widget);

	eog_message_area_response (message_area, response_id);
}

/**
 * eog_message_area_add_action_widget:
 * @message_area: an #EogMessageArea.
 * @child: The widget to be packed into the message area.
 * @response_id: A response id for @child.
 *
 * Adds a widget to the action area of @message_area. Only 'activatable'
 * widgets can be packed into the action area of a #EogMessageArea.
 **/
void
eog_message_area_add_action_widget (EogMessageArea *message_area,
				    GtkWidget      *child,
				    gint            response_id)
{
	ResponseData *ad;
	guint signal_id;

	g_return_if_fail (EOG_IS_MESSAGE_AREA (message_area));
	g_return_if_fail (GTK_IS_WIDGET (child));

	ad = get_response_data (child, TRUE);

	ad->response_id = response_id;

	if (GTK_IS_BUTTON (child)) {
		signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
	} else {
		signal_id = GTK_WIDGET_GET_CLASS (child)->activate_signal;
	}

	if (signal_id) {
		GClosure *closure;

		closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
						 G_OBJECT (message_area));

		g_signal_connect_closure_by_id (child,
						signal_id,
						0,
						closure,
						FALSE);
	} else {
		g_warning ("Only 'activatable' widgets can be packed into the action area of a EogMessageArea");
	}

	if (response_id != GTK_RESPONSE_HELP) {
		gtk_box_pack_start (GTK_BOX (message_area->priv->action_area),
				    child,
				    FALSE,
				    FALSE,
				    0);
	} else {
		gtk_box_pack_end (GTK_BOX (message_area->priv->action_area),
				    child,
				    FALSE,
				    FALSE,
				    0);
	}
}

/**
 * eog_message_area_set_contents:
 * @message_area: an #EogMessageArea.
 * @contents: a #GtkWidget.
 *
 * Sets the contents of @message_area. The @contents will
 * be packed into the message area.
 **/
void
eog_message_area_set_contents(EogMessageArea *message_area,
			      GtkWidget      *contents)
{
	g_return_if_fail (EOG_IS_MESSAGE_AREA (message_area));
	g_return_if_fail (GTK_IS_WIDGET (contents));

  	message_area->priv->contents = contents;

	gtk_box_pack_start (GTK_BOX (message_area->priv->main_hbox),
			    message_area->priv->contents,
			    TRUE,
			    TRUE,
			    0);
}

/**
 * eog_message_area_add_button:
 * @message_area: an #EogMessageArea.
 * @button_text: The text for the button to be added.
 * @response_id: A response id.
 *
 * Adds a single button to @message_area.
 *
 * Returns: The newly added #GtkButton.
 **/
GtkWidget *
eog_message_area_add_button (EogMessageArea *message_area,
			       const gchar      *button_text,
			       gint              response_id)
{
	GtkWidget *button;

	g_return_val_if_fail (EOG_IS_MESSAGE_AREA (message_area), NULL);
	g_return_val_if_fail (button_text != NULL, NULL);

	button = gtk_button_new_from_stock (button_text);

	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

	gtk_widget_show (button);

	eog_message_area_add_action_widget (message_area,
					    button,
					    response_id);

	return button;
}

static void
add_buttons_valist (EogMessageArea *message_area,
		    const gchar    *first_button_text,
		    va_list         args)
{
	const gchar* text;
	gint response_id;

	g_return_if_fail (EOG_IS_MESSAGE_AREA (message_area));

	if (first_button_text == NULL)
		return;

	text = first_button_text;
	response_id = va_arg (args, gint);

	while (text != NULL) {
		eog_message_area_add_button (message_area,
					     text,
					     response_id);

		text = va_arg (args, gchar*);

		if (text == NULL) break;

		response_id = va_arg (args, int);
	}
}

/**
 * eog_message_area_add_buttons:
 * @message_area: An #EogMessageArea.
 * @first_button_text: the text for the first button to be added.
 * @...: text for extra buttons, terminated with %NULL.
 *
 * Adds one or more buttons to @message_area.
 **/
void
eog_message_area_add_buttons (EogMessageArea *message_area,
			      const gchar    *first_button_text,
			      ...)
{
	va_list args;

	va_start (args, first_button_text);

	add_buttons_valist (message_area,
                            first_button_text,
                            args);

	va_end (args);
}

GtkWidget *
eog_message_area_new (void)
{
	return g_object_new (EOG_TYPE_MESSAGE_AREA, NULL);
}

/**
 * eog_message_area_new_with_buttons:
 * @first_button_text: The text for the first button.
 * @...: additional buttons, terminated with %NULL.
 *
 * Creates a new #EogMessageArea with default buttons.
 *
 * Returns: A newly created #EogMessageArea widget.
 **/
GtkWidget *
eog_message_area_new_with_buttons (const gchar *first_button_text,
                                   ...)
{
	EogMessageArea *message_area;
	va_list args;

	message_area = EOG_MESSAGE_AREA (eog_message_area_new ());

	va_start (args, first_button_text);

	add_buttons_valist (message_area,
			    first_button_text,
			    args);

	va_end (args);

	return GTK_WIDGET (message_area);
}

/**
 * eog_message_area_set_response_sensitive:
 * @message_area: a #EogMessageArea.
 * @response_id: the response id associated to the widget whose
 * sensitivity is to be set.
 * @setting: %TRUE to set the widget sensitive, %FALSE otherwise.
 *
 * Sets sensitivity in @message_area's child widget associated to
 * @response_id.
 **/
void
eog_message_area_set_response_sensitive (EogMessageArea *message_area,
					   gint              response_id,
					   gboolean          setting)
{
	GList *children;
	GList *tmp_list;

	g_return_if_fail (EOG_IS_MESSAGE_AREA (message_area));

	children = gtk_container_get_children (GTK_CONTAINER (message_area->priv->action_area));

	tmp_list = children;

	while (tmp_list != NULL) {
		GtkWidget *widget = tmp_list->data;
		ResponseData *rd = get_response_data (widget, FALSE);

		if (rd && rd->response_id == response_id)
			gtk_widget_set_sensitive (widget, setting);

		tmp_list = g_list_next (tmp_list);
	}

	g_list_free (children);
}

/**
 * eog_message_area_set_default_response:
 * @message_area: an #EogMessageArea.
 * @response_id: the response id associated to the widget to
 * be set default.
 *
 * Sets the default response in @message_area. This is done by
 * making the widget associated to @response_id the default widget.
 **/
void
eog_message_area_set_default_response (EogMessageArea *message_area,
					 gint              response_id)
{
	GList *children;
	GList *tmp_list;

	g_return_if_fail (EOG_IS_MESSAGE_AREA (message_area));

	children = gtk_container_get_children (GTK_CONTAINER (message_area->priv->action_area));

	tmp_list = children;
	while (tmp_list != NULL) {
		GtkWidget *widget = tmp_list->data;
		ResponseData *rd = get_response_data (widget, FALSE);

		if (rd && rd->response_id == response_id)
			gtk_widget_grab_default (widget);

		tmp_list = g_list_next (tmp_list);
	}

	g_list_free (children);
}

/**
 * eog_message_area_response:
 * @message_area: an #EogMessageArea.
 * @response_id: The response id for the emission.
 *
 * Emits a #EogMessageArea::response signal to the given #EogMessageArea.
 **/
void
eog_message_area_response (EogMessageArea *message_area,
			   gint              response_id)
{
	g_return_if_fail (EOG_IS_MESSAGE_AREA (message_area));

	g_signal_emit (message_area,
		       signals[SIGNAL_RESPONSE],
		       0,
		       response_id);
}

/**
 * eog_message_area_add_stock_button_with_text:
 * @message_area: An #EogMessageArea.
 * @text: The text for the button.
 * @stock_id: A stock item.
 * @response_id: A response id for the button.
 *
 * Adds a new button to @message_area containing a image from stock and a
 * user defined text. Stock items may have a macro defined, like %GTK_STOCK_OK.
 *
 * Returns: the newly added #GtkButton.
 **/
GtkWidget *
eog_message_area_add_stock_button_with_text (EogMessageArea *message_area,
				    	     const gchar    *text,
				    	     const gchar    *stock_id,
				    	     gint            response_id)
{
	GtkWidget *button;

	g_return_val_if_fail (EOG_IS_MESSAGE_AREA (message_area), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (stock_id != NULL, NULL);

	button = gtk_button_new_with_mnemonic (text);

        gtk_button_set_image (GTK_BUTTON (button),
                              gtk_image_new_from_stock (stock_id,
                                                        GTK_ICON_SIZE_BUTTON));

	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

	gtk_widget_show (button);

	eog_message_area_add_action_widget (message_area,
					    button,
					    response_id);

	return button;
}

