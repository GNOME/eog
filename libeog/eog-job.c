#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-job.h"

static guint last_job_id = 0;
#define DEBUG_EOG_JOB 0

enum {
	PROP_0,
	PROP_PROGRESS_THRESHOLD,
	PROP_PROGRESS_N_PARTS,
	PROP_PRIORITY
};

struct _EogJobPrivate {
	GMutex        *mutex;
	guint         id;
	gpointer      data;
	EogJobStatus  status;
	GError        *error;
	float         progress;
	float         progress_last_called;
	guint         progress_idle_id;
	guint         progress_nth_part;
	guint         progress_n_parts;
	float         progress_threshold;
	guint         priority;
	
	EogJobActionFunc af;
	EogJobCancelFunc cf;
	EogJobFinishedFunc ff;
	EogJobProgressFunc pf;
	EogJobFreeDataFunc df;
};

G_DEFINE_TYPE (EogJob, eog_job,	G_TYPE_OBJECT)

static void
eog_job_set_property (GObject      *object,
		      guint         property_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	EogJob *job;

	g_return_if_fail (EOG_IS_JOB (object));

	job = EOG_JOB (object);

	switch (property_id) {
	case PROP_PROGRESS_THRESHOLD:
		job->priv->progress_threshold = 
			g_value_get_float (value);
		break;
	case PROP_PROGRESS_N_PARTS:
		job->priv->progress_n_parts = 
			g_value_get_uint (value);
		break;
	case PROP_PRIORITY:
		job->priv->priority = 
			g_value_get_uint (value);
		break;
        default:
                g_assert_not_reached ();
	}
}

static void
eog_job_get_property (GObject    *object,
		      guint       property_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	EogJob *job;

	g_return_if_fail (EOG_IS_JOB (object));

	job = EOG_JOB (object);

	switch (property_id) {
	case PROP_PROGRESS_THRESHOLD:
		g_value_set_float (value,
				   job->priv->progress_threshold);
		break;
	case PROP_PROGRESS_N_PARTS:
		g_value_set_uint (value,				  
				  job->priv->progress_n_parts);
		break;
	case PROP_PRIORITY:
		g_value_set_uint (value,
				  job->priv->priority);
		break;
        default:
                g_assert_not_reached ();
	}
}

static void
eog_job_finalize (GObject *object)
{
	EogJob *instance = EOG_JOB (object);
	
	g_free (instance->priv);
	instance->priv = NULL;

	G_OBJECT_CLASS (eog_job_parent_class)->finalize (object);
}

static void
eog_job_dispose (GObject *object)
{
	EogJobPrivate *priv;

	priv = EOG_JOB (object)->priv;

#if DEBUG_EOG_JOB
	g_print ("Job %.3u: disposing ...\n", priv->id);
#endif

	if (priv->mutex != NULL) {
		g_mutex_lock (priv->mutex);
		if (priv->data != NULL)
		{
			if (priv->df != NULL) {
				(*priv->df) (priv->data);
			}
			else if (G_IS_OBJECT (priv->data))
				g_object_unref (G_OBJECT (priv->data));
			
			priv->data = NULL;
		}
		g_mutex_unlock (priv->mutex);
		
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

#if DEBUG_EOG_JOB
	g_print ("Job %.3u: disposing end\n", priv->id);
#endif
	G_OBJECT_CLASS (eog_job_parent_class)->dispose (object);
}

static void
eog_job_init (EogJob *obj)
{
	EogJobPrivate *priv;
	guint id;

	id = ++last_job_id;
	if (id == 0) { /* in overflow case */
		++last_job_id;
		++id; 
	}

#if DEBUG_EOG_JOB
	g_print ("Instantiate job with id %u.\n", id);
#endif

	priv = g_new0 (EogJobPrivate, 1);
	priv->id = id;
	priv->mutex = g_mutex_new ();
	priv->error = NULL;
	priv->data = NULL;
	priv->af = NULL;
	priv->cf = NULL;
	priv->ff = NULL;
	priv->pf = NULL;
	priv->df = NULL;
	priv->progress = 0.0;
	priv->progress_idle_id = 0;
	priv->progress_threshold = 0.1;
	priv->progress_n_parts = 1;
	priv->priority = EOG_JOB_PRIORITY_NORMAL;

	obj->priv = priv;
}

static void 
eog_job_class_init (EogJobClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_job_finalize;
	object_class->dispose = eog_job_dispose;
        object_class->set_property = eog_job_set_property;
        object_class->get_property = eog_job_get_property;

        /* Properties */
        g_object_class_install_property (
                object_class,
                PROP_PROGRESS_THRESHOLD,
                g_param_spec_float ("progress-threshold", NULL, 
				    "Difference threshold between two progress values"
				    "before calling progress callback function again", 
				    0.0, 1.0, 0.1,
				    G_PARAM_READWRITE));

        g_object_class_install_property (
                object_class,
                PROP_PROGRESS_N_PARTS,
                g_param_spec_uint ("progress-n-parts", NULL,
				   "Number of parts for this progress so that "
				   "the total progress is the sum of progress "
				   "for each part devided through number of parts.",
				   0, G_MAXINT, 1,
				   G_PARAM_READWRITE));

        g_object_class_install_property (
                object_class,
                PROP_PRIORITY,
                g_param_spec_uint ("priority", NULL,
				   "Priority of the job.",
				   0, G_MAXINT, 1,
				   G_PARAM_WRITABLE));
}

EogJob*             
eog_job_new (GObject *data_obj, 
	     EogJobActionFunc   af, 
	     EogJobFinishedFunc ff,
	     EogJobCancelFunc   cf,
	     EogJobProgressFunc pf)
{
	g_return_val_if_fail (G_IS_OBJECT (data_obj), NULL);

	return eog_job_new_full ((gpointer) data_obj, af, ff, cf, pf, NULL);
}

/* called from main thread */
EogJob*             
eog_job_new_full (gpointer data, 
		  EogJobActionFunc   af, 
		  EogJobFinishedFunc ff,
		  EogJobCancelFunc   cf,
		  EogJobProgressFunc pf,
		  EogJobFreeDataFunc df)
{
	EogJob *job;
	EogJobPrivate *priv;

	g_return_val_if_fail (af != NULL, NULL);

	job = g_object_new (EOG_TYPE_JOB, NULL);
	priv = job->priv;

	if (G_IS_OBJECT (data)) {
		g_object_ref (G_OBJECT (data));
	}
	priv->data = data;
	priv->status = EOG_JOB_STATUS_WAITING;
	priv->af = af;
	priv->ff = ff;
	priv->cf = cf;
	priv->pf = pf;
	priv->df = df;
	
	return job;
}

/* called from main thread */
EogJobStatus
eog_job_get_status (EogJob *job)
{
	g_return_val_if_fail (EOG_IS_JOB (job), EOG_JOB_STATUS_ERROR);

	return job->priv->status;
}

/* called from main thread */
guint
eog_job_get_id (EogJob *job)
{
	g_return_val_if_fail (EOG_IS_JOB (job), 0);

	return job->priv->id;
}

/* called from main thread */
gboolean
eog_job_get_success (EogJob *job) 
{
	gboolean success;

	g_return_val_if_fail (EOG_IS_JOB (job), FALSE);
	
	g_mutex_lock (job->priv->mutex);
	
	success = ((job->priv->status == EOG_JOB_STATUS_FINISHED) && 
		   (job->priv->error == NULL));

	g_mutex_unlock (job->priv->mutex);

	return success;
}

void
eog_job_part_finished (EogJob *job)
{
	g_return_if_fail (EOG_IS_JOB (job));

	job->priv->progress_nth_part = 
		MIN ((job->priv->progress_nth_part + 1),
		     job->priv->progress_n_parts);
}

/* private func, called within main thread from idle loop */
static gboolean
eog_job_call_progress (gpointer data)
{
	EogJob *job = EOG_JOB (data);

	g_assert (job->priv->pf != NULL);

	g_mutex_lock (job->priv->mutex);
	job->priv->progress_idle_id = 0;
	g_mutex_unlock (job->priv->mutex);

	/* call progress callback */
	(*job->priv->pf) (job, job->priv->data, job->priv->progress);

	return FALSE;
}

/* called from concurrent thread */
void
eog_job_set_progress (EogJob *job, float progress)
{
	EogJobPrivate *priv;

	g_return_if_fail (EOG_IS_JOB (job));

	priv = job->priv;

	g_mutex_lock (job->priv->mutex);
	/* calculate new progress */
	priv->progress = ((float) priv->progress_nth_part + progress) /
		priv->progress_n_parts;
	
	/* check if all preconditions are met for calling progress callback */
	if ((priv->pf != NULL) && 
	    (priv->progress_idle_id == 0) && 
	    (priv->progress == 1.0 ||
	     (priv->progress - priv->progress_last_called) >= priv->progress_threshold))
	{
		priv->progress_last_called = priv->progress;
		priv->progress_idle_id = g_idle_add (eog_job_call_progress, job);
	}
	g_mutex_unlock (job->priv->mutex);
}

/* this runs in its own thread 
 * called by EogJobManager
 */
void
eog_job_call_action (EogJob *job)
{
	gboolean do_action = TRUE;

	g_return_if_fail (EOG_IS_JOB (job));
	
	g_mutex_lock (job->priv->mutex);
	if (job->priv->status != EOG_JOB_STATUS_WAITING)
		do_action = FALSE;
	
	if (job->priv->af == NULL) {
		job->priv->status = EOG_JOB_STATUS_ERROR;
		do_action = FALSE;
	}
	
	if (do_action) {
		job->priv->status = EOG_JOB_STATUS_RUNNING;
		job->priv->progress = 0.0;
		job->priv->progress_last_called = 0.0;
		job->priv->progress_nth_part = 0;
	}

	g_mutex_unlock (job->priv->mutex);

	if (!do_action) {
		return;
	}

	/* do the actual work */
	(*job->priv->af) (job, job->priv->data, &job->priv->error);

	g_mutex_lock (job->priv->mutex);
	if (job->priv->status != EOG_JOB_STATUS_CANCELED)
		job->priv->status = EOG_JOB_STATUS_FINISHED;
	g_mutex_unlock (job->priv->mutex);
}

/* called within main thread from idle loop 
 * by EogJobManager
 */
gboolean 
eog_job_call_canceled (EogJob *job)
{
	gboolean do_cancel = FALSE;
	gboolean valid_state = FALSE;

	g_return_val_if_fail (EOG_IS_JOB (job), FALSE);

	g_mutex_lock (job->priv->mutex);
	valid_state = (job->priv->status == EOG_JOB_STATUS_WAITING  ||
		       job->priv->status == EOG_JOB_STATUS_RUNNING); 
	do_cancel = (job->priv->cf != NULL && valid_state);
	if (valid_state) {
		job->priv->status = EOG_JOB_STATUS_CANCELED;
	}
	g_mutex_unlock (job->priv->mutex);
	
	if (do_cancel) {
		/* call cancel function */
		(*job->priv->cf) (job, job->priv->data);
	}

	return valid_state;
}

/* called within main thread from idle loop 
 * by EogJobManager
 */
void
eog_job_call_finished (EogJob *job)
{
	gboolean call_finished = FALSE;

	g_return_if_fail (EOG_IS_JOB (job));

	g_mutex_lock (job->priv->mutex);
	g_assert (job->priv->status == EOG_JOB_STATUS_FINISHED ||
		  job->priv->status == EOG_JOB_STATUS_CANCELED);

	call_finished = (job->priv->ff != NULL);
	g_mutex_unlock (job->priv->mutex);

	if (call_finished) {
		/* call finished callback */
		(*job->priv->ff) (job, job->priv->data, job->priv->error);
	}
}

EogJobPriority      
eog_job_get_priority (EogJob *job)
{
	g_return_val_if_fail (EOG_IS_JOB (job), EOG_JOB_PRIORITY_NORMAL);

	return job->priv->priority;
}
