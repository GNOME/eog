#include "eog-jobs.h"
#include "eog-job-queue.h"

GMutex *eog_queue_mutex = NULL;

static GQueue *thumbnail_queue = NULL;
static GQueue *load_queue = NULL;
static GQueue *model_queue = NULL;
static GQueue *transform_queue = NULL;

static gboolean
remove_job_from_queue (GQueue *queue, EogJob *job)
{
	GList *list;

	list = g_queue_find (queue, job);

	if (list) {
		g_object_unref (G_OBJECT (job));
		g_queue_delete_link (queue, list);

		return TRUE;
	}

	return FALSE;
}

static void
add_job_to_queue (GQueue *queue, EogJob  *job)
{
	g_object_ref (job);
	g_queue_push_tail (queue, job);
}

static gboolean
notify_finished (GObject *job)
{
	eog_job_finished (EOG_JOB (job));

	return FALSE;
}

static void
handle_job (EogJob *job)
{
	g_object_ref (G_OBJECT (job));

	if (EOG_IS_JOB_THUMBNAIL (job))
		eog_job_thumbnail_run (EOG_JOB_THUMBNAIL (job));
	else if (EOG_IS_JOB_LOAD (job))
		eog_job_load_run (EOG_JOB_LOAD (job));
	else if (EOG_IS_JOB_MODEL (job))
		eog_job_model_run (EOG_JOB_MODEL (job));
	else if (EOG_IS_JOB_TRANSFORM (job))
		eog_job_transform_run (EOG_JOB_TRANSFORM (job));

	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			 (GSourceFunc) notify_finished,
			 job,
			 g_object_unref);
}

static EogJob *
search_for_jobs_unlocked (void)
{
	EogJob *job;

	job = (EogJob *) g_queue_pop_head (load_queue);
	if (job)
		return job;

	job = (EogJob *) g_queue_pop_head (transform_queue);
	if (job)
		return job;

	job = (EogJob *) g_queue_pop_head (thumbnail_queue);
	if (job)
		return job;

	job = (EogJob *) g_queue_pop_head (model_queue);
	if (job)
		return job;

	return NULL;
}

static gpointer
eog_render_thread (gpointer data)
{
	while (TRUE) {
		EogJob *job;

		g_mutex_lock (eog_queue_mutex);

		job = search_for_jobs_unlocked ();

		g_mutex_unlock (eog_queue_mutex);

		/* Now that we have our job, we handle it */
		if (job) {
			handle_job (job);
			g_object_unref (G_OBJECT (job));
		}
	}
	return NULL;

}

void
eog_job_queue_init (void)
{
	if (!g_thread_supported ()) g_thread_init (NULL);

	eog_queue_mutex = g_mutex_new ();

	thumbnail_queue = g_queue_new ();
	load_queue = g_queue_new ();
	model_queue = g_queue_new ();
	transform_queue = g_queue_new ();

	g_thread_create (eog_render_thread, NULL, FALSE, NULL);
}

static GQueue *
find_queue (EogJob *job)
{
	if (EOG_IS_JOB_THUMBNAIL (job)) {
		return thumbnail_queue;
	} else if (EOG_IS_JOB_LOAD (job)) {
		return load_queue;
	} else if (EOG_IS_JOB_MODEL (job)) {
		return model_queue;
	} else if (EOG_IS_JOB_TRANSFORM (job)) {
		return transform_queue;
	}

	g_assert_not_reached ();

	return NULL;
}

void
eog_job_queue_add_job (EogJob *job)
{
	GQueue *queue;

	g_return_if_fail (EOG_IS_JOB (job));

	queue = find_queue (job);

	g_mutex_lock (eog_queue_mutex);
	
	add_job_to_queue (queue, job);

	g_mutex_unlock (eog_queue_mutex);
}

gboolean
eog_job_queue_remove_job (EogJob *job)
{
	gboolean retval = FALSE;

	g_return_val_if_fail (EOG_IS_JOB (job), FALSE);

	g_mutex_lock (eog_queue_mutex);

	if (EOG_IS_JOB_THUMBNAIL (job)) {
		retval = remove_job_from_queue (thumbnail_queue, job);
	} else if (EOG_IS_JOB_LOAD (job)) {
		retval = remove_job_from_queue (load_queue, job);
	} else if (EOG_IS_JOB_MODEL (job)) {
		retval = remove_job_from_queue (model_queue, job);
	} else {
		g_assert_not_reached ();
	}

	g_mutex_unlock (eog_queue_mutex);
	
	return retval;
}
