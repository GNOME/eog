/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8 c-style: "K&R" -*- */
/**
 * eog-image-selection-model.c
 *
 * Authors:
 *   Jens Finke (jens@gnome.org)
 *
 * Copyright 2000 The Free Software Foundation
 */
#include <gtk/gtk.h>
#include "eog-image-selection-model.h"
#include <libgnome/libgnome.h>

typedef struct {
	/* start number */
	gint start;
	/* length */
	gint length;
} Interval;


struct _EogImageSelectionModelPrivate {
	/* holds a sorted list of Interval*
	 * objects.
	 */
	GList *sel_intervals;
};


static void eog_image_selection_model_class_init (EogImageSelectionModelClass *class);
static void eog_image_selection_model_init (EogImageSelectionModel *model);
static void eog_image_selection_model_destroy (GtkObject *object);

static GnomeListSelectionModelClass *parent_class;

/* Query implementation from GnomeListModel. */
static guint eism_get_length (GnomeListModel *model, gpointer data);

/* Mutation implementations from GnomeListSelectionModel. */
static void eism_clear (GnomeListSelectionModel *model, gpointer data);
static void eism_set_interval (GnomeListSelectionModel *model, guint start, guint length, gpointer data);
static void eism_add_interval (GnomeListSelectionModel *model, guint start, guint length, gpointer data);
static void eism_remove_interval (GnomeListSelectionModel *model, guint start, guint length, gpointer data);

/* Query implementations from GnomeListSelectionModel. */
static gboolean eism_is_selected (GnomeListSelectionModel *model, guint n, gpointer data);
static gint eism_get_min_selected (GnomeListSelectionModel *model, gpointer data);
static gint eism_get_max_selected (GnomeListSelectionModel *model, gpointer data);


/**
 * eog_image_selection_model_get_type:
 * @void:
 *
 * Registers the #EogImageSelectionModel class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #EogImageSelectionModel class.
 **/
GtkType 
eog_image_selection_model_get_type(void)
{
	static GtkType eog_image_selection_model_type = 0;

	if (!eog_image_selection_model_type) {
		static const GtkTypeInfo eog_image_selection_model_info = {
			"EogImageSelectionModel",
			sizeof (EogImageSelectionModel),
			sizeof (EogImageSelectionModelClass),
			(GtkClassInitFunc) eog_image_selection_model_class_init,
			(GtkObjectInitFunc) eog_image_selection_model_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		eog_image_selection_model_type = gtk_type_unique (GNOME_TYPE_LIST_SELECTION_MODEL, 
							     &eog_image_selection_model_info);
	}

	return eog_image_selection_model_type;
}


/* Class initialization function for the image selection model. */
void
eog_image_selection_model_class_init(EogImageSelectionModelClass *klass)
{
	GtkObjectClass *obj_klass;

	parent_class = gtk_type_class(gnome_list_selection_model_get_type ());

	obj_klass = (GtkObjectClass*) klass;
	obj_klass->destroy = eog_image_selection_model_destroy;
}


/* Object initialization function for the image selection model. */
void
eog_image_selection_model_init(EogImageSelectionModel *model)
{
	EogImageSelectionModelPrivate *priv;

	priv = g_new0(EogImageSelectionModelPrivate, 1);
	model->priv = priv;
}

/* Destroy handler for the image selection model. */
void 
eog_image_selection_model_destroy (GtkObject *object)
{
	EogImageSelectionModel *model;
	EogImageSelectionModelPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (object));

	model = EOG_IMAGE_SELECTION_MODEL(object);
	priv = model->priv;

	eism_clear (GNOME_LIST_SELECTION_MODEL (model), NULL);
	g_free (priv);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/**
 * eog_image_selection_model_new:
 * @void:
 *
 * Creates a new image selection model implementation. 
 *
 * Return value: The newly-created object.
 **/
GtkObject*
eog_image_selection_model_new(void)
{
	EogImageSelectionModel *model;

	model = EOG_IMAGE_SELECTION_MODEL (gtk_type_new (TYPE_EOG_IMAGE_SELECTION_MODEL));
	model->priv->sel_intervals = NULL;

	/* connect interface signals */
	gtk_signal_connect (GTK_OBJECT (model), "get_length",
			    GTK_SIGNAL_FUNC (eism_get_length),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "clear",
			    GTK_SIGNAL_FUNC (eism_clear),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "set_interval",
			    GTK_SIGNAL_FUNC (eism_set_interval),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "add_interval",
			    GTK_SIGNAL_FUNC (eism_add_interval),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "remove_interval",
			    GTK_SIGNAL_FUNC (eism_remove_interval),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "is_selected",
			    GTK_SIGNAL_FUNC (eism_is_selected),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "get_min_selected",
			    GTK_SIGNAL_FUNC (eism_get_min_selected),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (model), "get_max_selected",
			    GTK_SIGNAL_FUNC (eism_get_max_selected),
			    NULL);

	return GTK_OBJECT(model);
}

/* Queries the number of images that are managed in this model. */
/* Implementation of gnome_list_model_get_length. */
guint 
eism_get_length (GnomeListModel *model, gpointer data)
{
	GList *list = NULL;
	gint result = 0;
	Interval *iv;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model), 0);

	list = EOG_IMAGE_SELECTION_MODEL (model)->priv->sel_intervals;

	while (list) {
		iv = (Interval*) list->data;
		result += iv->length;
		list = list->next;
	}
	
	return result;
}

/* callback function for g_list_foreach to free a Interval struct. */
static void
destroy_interval (Interval *interval, gpointer data)
{
	g_free (interval);
}

/* compares two Interval objects (used for g_list_insert_sorted). */
static gint
compare_interval (Interval *a, Interval *b)
{
	return (a->start - b->start);
}

/*
 * Mutation implementations from GnomeListSelectionModel.
 */
/* Clears all intervals managed by this model. 
 * Implementation of gnome_list_selection_model_clear).
 */
void 
eism_clear (GnomeListSelectionModel *model, gpointer data)
{
	EogImageSelectionModelPrivate *priv;
	gint min, max;
	
	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model));

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;

	min = eism_get_min_selected (model, NULL);
	max = eism_get_max_selected (model, NULL);

	if(priv->sel_intervals == NULL) return;

	g_list_foreach (priv->sel_intervals, (GFunc) destroy_interval, NULL);
	g_list_free (priv->sel_intervals);
	priv->sel_intervals = NULL;

	gnome_list_model_interval_removed (GNOME_LIST_MODEL (model), min, max-min+1);
}

/* Sets the internal interval list to start/lenght and deletes all previous existing 
 * intervals. Implementation of gnome_list_selection_model_set_interval.
 */
void 
eism_set_interval (GnomeListSelectionModel *model, guint start, guint length, gpointer data)
{
	EogImageSelectionModelPrivate *priv;
	Interval *interval;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model));

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;

	eism_clear (model, NULL);

	interval = (Interval*) g_new0 (Interval*, 1);
	interval->start = start;
	interval->length = length;

	priv->sel_intervals = g_list_append (priv->sel_intervals, interval);

	gnome_list_model_interval_added (GNOME_LIST_MODEL (model), start, length);
}

/* Merges two intervals if the second one follows directly the first one.
 * e.g: merges [4-6] and [7-8] to [4-8]. Or [4-7] and [5-10] to [4-10].
 *      The intervals [4-7] and [10-9] won't be merged.
 */
static Interval*
try_merge (Interval *first, Interval *second)
{
	Interval *merged;

	g_return_val_if_fail (first != NULL, NULL);
	g_return_val_if_fail (second != NULL, NULL);
	g_assert (first->start <= second->start);

	if (first->start + first->length < second->start) return NULL;

	merged = (Interval*) g_new0 (Interval*, 1);

	if (first->start + first->length > second->start + second->length) {
		merged->start = first->start;
		merged->length = first->length;
	} else {
		merged->start = first->start;
		merged->length = second->start - first->start + second->length;
	}

	return merged;
}

/* Adds an interval to the internal list. Implementation of 
 * gnome_list_selection_model_add_interval.
 */
void 
eism_add_interval (GnomeListSelectionModel *model, guint start, guint length, gpointer data)
{
	EogImageSelectionModelPrivate *priv;
	GList *result;
	GList *prev;
	GList *next;
	Interval *interval;
	Interval *merged;
	gboolean rm_prev, rm_next;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model));

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;

	/* create interval */
	interval = (Interval*) g_new0 (Interval*, 1);
	interval->start = start;
	interval->length = length;
	
	/* insert interval */
	priv->sel_intervals = g_list_insert_sorted (priv->sel_intervals,
						    interval,
						    (GCompareFunc) compare_interval);
	result = g_list_find (priv->sel_intervals, interval);
	g_assert (result != NULL);

	prev = result->prev;
	next = result->next;
	
	/* merge neighbourhood intervals if possible */
	rm_prev = rm_next = FALSE;

	if(prev) {
		merged = try_merge (prev->data, interval);
		if (merged != NULL) {
			interval->start = merged->start;
			interval->length = merged->length;
			rm_prev = TRUE;
			g_free (merged);
		}
	}
	
	if(next) {
		merged = try_merge (interval, next->data);
		if (merged != NULL) {
			interval->start = merged->start;
			interval->length = merged->length;
			rm_next = TRUE;
			g_free (merged);
		}
	}

	/* remove intervals if neccessary */
	if (rm_prev) {
		priv->sel_intervals = g_list_remove_link (priv->sel_intervals, prev);
	        g_free (prev->data);
		g_list_free (prev);
	}

	if (rm_next) {
		priv->sel_intervals = g_list_remove_link (priv->sel_intervals, next);
		g_free (next->data);
		g_list_free (next);
	}

	gnome_list_model_interval_added (GNOME_LIST_MODEL (model), start, length);
}

/* Removes all numbers in the specified interval from the internal interval list. 
 * Implementation of gnome_list_selection_model_remove_interval. 
 */
void 
eism_remove_interval (GnomeListSelectionModel *model, guint start, guint length, gpointer data)
{
	EogImageSelectionModelPrivate *priv;

	guint end;
	gint max, min;
	GList *list = NULL;
	GList *tmp = NULL;
	GList *start_change = NULL;
	GList *end_change = NULL;
	GList *intersec_change = NULL;
	GList *first_del = NULL;
	GList *last_del = NULL;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model));
	if (length == 0) return;

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;

	end = start + length;
	min = eism_get_min_selected (model, NULL);
	max = eism_get_max_selected (model, NULL);

	/* if the rm-interval affects no saved selection interval */
	if (end < min || start > max) return;

	/* if the rm-interval contains all saved intervals */
	if (start <= min && end >= max) {
		eism_clear (model, NULL);
		return;
	}

	/* else find intervals which need to be delete or
	   change */
	list = g_list_first (priv->sel_intervals);
	while (list != NULL) {
		Interval *i;
		gint is, ie;

		i = (Interval*) list->data;
		is = i->start;
		ie = i->start + i->length;

		if (is < start && end < ie) {   
                        /* interval i contains the whole rm-interval */
			intersec_change = list;
			break;
		} else if (is == start && end == ie) {
			/* interval i is equal rm-interval */
			first_del = list;
			break;
		} else if (start <= is && ie <= end) {
			/* rm-interval contains the whole interval i */
			if(!first_del)
				first_del = list;
			else
				last_del = list;
		} else if (is >= start && is < end) {
			/* beginning of interval i is contained in rm-interval */
			start_change = list;
		} else if (ie >= start && ie <= end) {
			/* end of interval i is contained in rm-interval */
			end_change = list;
		} else if (is > end)
			/* interval i beginning is out of 
			   rm-interval */
			break;

		list = list->next;
	}

	/* change appropriate intervals */
	if (start_change)  {
		Interval *i = NULL;
		i = (Interval*) start_change->data;
		i->length = (i->start + i->length) - end;
		i->start = end;
	}
	
	if (end_change) {
		Interval *i = NULL;
		i = (Interval*) end_change->data;
		i->length = start - i->start;
	}

	if (intersec_change) {
		Interval *i;
		gint tmplen;
		gint tmpstart;

		i = (Interval*) intersec_change->data; 
		tmpstart = i->start;
		tmplen = i->length;
		i->length = start - i->start;
		
		i = (Interval*) g_new0 (Interval*, 1);
		i->start = end;
		i->length = tmpstart + tmplen - end;
		
		g_list_insert_sorted (priv->sel_intervals, 
				      i, (GCompareFunc) compare_interval);
	}

	/* delete appropriate intervals */
	if (first_del) {
		list = first_del;
		do {		
			tmp = list->next;
			priv->sel_intervals = g_list_remove_link (priv->sel_intervals, list);
			g_free (list->data);
			g_list_free (list);
			list = tmp;
			if (last_del == NULL) break;
		} while (list != last_del);
	}

	gnome_list_model_interval_removed (GNOME_LIST_MODEL (model), start, length);
}

/* 
 * Query implementations from GnomeListSelectionModel. 
 */
/* Queries the model if number n is contained in any of the saved intervals. 
 * Implementation of gnome_list_selection_model_is_selected.
 */
gboolean 
eism_is_selected (GnomeListSelectionModel *model, guint n, gpointer data)
{
	EogImageSelectionModelPrivate *priv;
	GList *list;
	gint start, length;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model), FALSE);

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;
	
	list = g_list_last (priv->sel_intervals);
	if (list == NULL) return FALSE;

	start = ((Interval*)list->data)->start;

	while (start > n && list) {
		list = list->prev;
		if (list == NULL) return FALSE;
		start = ((Interval*)list->data)->start;
	}
	
	length = ((Interval*)list->data)->length;
	return (start <= n && n < (start+length));
}

/* Returns the minimal selected number.
 * Implementation of gnome_list_selection_get_min_selected. 
 */
gint 
eism_get_min_selected (GnomeListSelectionModel *model, gpointer data)
{
	EogImageSelectionModelPrivate *priv;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model), 0);

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;
	
	if (priv->sel_intervals == NULL) return 0;

	return ((Interval*)priv->sel_intervals->data)->start;
}

/* Returns the maximal selected number.
 * Implementation of gnome_list_selection_get_max_selected. 
 */
gint 
eism_get_max_selected (GnomeListSelectionModel *model, gpointer data)
{
	EogImageSelectionModelPrivate *priv;
	Interval *last;

	g_return_val_if_fail (model != NULL, 0);
	g_return_val_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model), 0);

	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;
	
	if (priv->sel_intervals == NULL) return 0;

	last = (Interval*)g_list_last (priv->sel_intervals)->data;

	return last->start + last->length - 1;
}

#ifdef DEBUG
/* internal debug function */ 
void 
eog_image_selection_model_print_debug (EogImageSelectionModel *model)
{
	EogImageSelectionModelPrivate *priv;
	GList *list;

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_EOG_IMAGE_SELECTION_MODEL (model));
	
	priv = EOG_IMAGE_SELECTION_MODEL (model)->priv;
	list = priv->sel_intervals;

	g_print ("\n--------- DEBUG INFOS ---------\n");
	while (list) {
		Interval *i;
		i = (Interval*) list->data;
		g_print ("[S:%i, L: %i]\n", i->start, i->length);
		list = list->next;
	}	
}
#endif


