#include <libgnome/gnome-macros.h>

#include "eog-job.h"

static guint last_job_id = 0;
#define DEBUG_EOG_JOB 1

struct _EogJobPrivate {
	GMutex        *mutex;
	guint         id;
	gpointer      data;
	EogJobStatus  status;
	GError        *error;
	float         progress;
	guint         progress_idle_id;
	
	EogJobActionFunc af;
	EogJobCancelFunc cf;
	EogJobFinishedFunc ff;
	EogJobProgressFunc pf;
	EogJobFreeDataFunc df;
};

static void
eog_job_finalize (GObject *object)
{
	EogJob *instance = EOG_JOB (object);
	
	g_free (instance->priv);
	instance->priv = NULL;
}

static void
eog_job_dispose (GObject *object)
{
	EogJobPrivate *priv;

	priv = EOG_JOB (object)->priv;

	g_print ("Job %.3i: disposing ...\n", priv->id);

	if (priv->mutex != NULL) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->data != NULL)
	{
		if (priv->df != NULL) 
			(*priv->df) (priv->data);
		else if (G_IS_OBJECT (priv->data))
			g_object_unref (G_OBJECT (priv->data));
		
		priv->data = NULL;
	}

	g_print ("Job %.3i: disposing end\n", priv->id);
}

static void
eog_job_instance_init (EogJob *obj)
{
	EogJobPrivate *priv;
	guint id;

	id = ++last_job_id;
	if (id == 0) { /* in overflow case */
		++last_job_id;
		++id; 
	}

#if DEBUG_EOG_JOB
	g_print ("Instantiate job with id %i.\n", id);
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

	obj->priv = priv;
}

static void 
eog_job_class_init (EogJobClass *klass)
{
	GObjectClass *object_class = (GObjectClass*) klass;

	object_class->finalize = eog_job_finalize;
	object_class->dispose = eog_job_dispose;
}


GNOME_CLASS_BOILERPLATE (EogJob,
			 eog_job,
			 GObject,
			 G_TYPE_OBJECT);

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

/* private func, called within main thread from idle loop */
static gboolean
eog_job_call_progress (gpointer data)
{
	EogJob *job = EOG_JOB (data);

	g_assert (job->priv->pf != NULL);

	/* call progress callback */
	(*job->priv->pf) (job, job->priv->data, job->priv->progress);

	job->priv->progress_idle_id = 0;
	return FALSE;
}


/* called from concurrent thread */
void
eog_job_set_progress (EogJob *job, float progress)
{
	gboolean call_progress = FALSE;

	g_return_if_fail (EOG_IS_JOB (job));

	g_mutex_lock (job->priv->mutex);
	call_progress = (job->priv->pf != NULL && job->priv->progress_idle_id == 0);
	g_mutex_unlock (job->priv->mutex);

	job->priv->progress = progress;
	if (call_progress) {
		job->priv->progress_idle_id = g_idle_add (eog_job_call_progress, job);
	}
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
