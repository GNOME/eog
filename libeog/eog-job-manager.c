#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-job-manager.h"

typedef struct {
	GThread *thread;
	EogJob  *job;
} ThreadData;

#define MAX_THREADS  2
#define DEBUG_JOB_MANAGER 0
GMutex        *mutex       = NULL;
static GCond  *cond        = NULL;
static GQueue *job_list    = NULL;
static guint  n_threads   = 0;
static ThreadData threads[MAX_THREADS];
static gboolean   init = FALSE;

/* Utility function, which initializes static data first */
static void
eog_job_manager_init ()
{
	if (!init) {
		int i;
		
		for (i = 0; i < MAX_THREADS; i++) {
			threads[i].thread = NULL;
			threads[i].job = NULL;
		}
		mutex = g_mutex_new ();
		cond = g_cond_new ();

		job_list = g_queue_new ();
		
		init = TRUE;
	}
}

/* idle callback from separate thread, at this
 * point control is in the main thread again
 */
static gboolean
job_finished_cb (gpointer data)
{
	EogJob *job;

	job = EOG_JOB (data);
	g_assert (eog_job_get_status (job) == EOG_JOB_STATUS_FINISHED ||
		  eog_job_get_status (job) == EOG_JOB_STATUS_CANCELED);

	eog_job_call_finished (job);

	g_assert (EOG_IS_JOB (job));

	g_object_unref (G_OBJECT (job));
	
	return FALSE;
}

/* this runs in its own thread */
static gpointer
thread_start_func (gpointer data)
{
	EogJob *job;
	guint thread_id = GPOINTER_TO_UINT (data);

#if DEBUG_JOB_MANAGER
	g_print ("Starting thread with id %u.\n", thread_id);
#endif

	while (TRUE) {
		job = NULL;
		
		g_mutex_lock (mutex);
		if (!g_queue_is_empty (job_list)) {
			job = EOG_JOB (g_queue_pop_head (job_list));
			threads[thread_id].job = job;
		}
		g_mutex_unlock (mutex);

		if (job != NULL) {
			if (eog_job_get_status (job) == EOG_JOB_STATUS_WAITING) {
				eog_job_call_action (job);
				
				g_assert (eog_job_get_status (job) == EOG_JOB_STATUS_FINISHED ||
					  eog_job_get_status (job) == EOG_JOB_STATUS_CANCELED);
			}
			
			if (eog_job_get_status (job) == EOG_JOB_STATUS_FINISHED ||
			    eog_job_get_status (job) == EOG_JOB_STATUS_CANCELED)
			{
				g_idle_add (job_finished_cb, job);
			}
		}
		else {
			g_mutex_lock (mutex);
			threads[thread_id].job = NULL;
			g_cond_wait (cond, mutex);
			g_mutex_unlock (mutex);
		}
	}

#if DEBUG_JOB_MANAGER
	g_print ("Stopping thread with id %u.\n", thread_id);
#endif
	/* This block will never be reached */
	/* clean up and remove thread */
	g_mutex_lock (mutex);
	n_threads--;
	threads[thread_id].thread = NULL;
	threads[thread_id].job = NULL;
	g_mutex_unlock (mutex);
	
	return NULL;
}

/* called from main thread */
guint     
eog_job_manager_add (EogJob *job)
{
	g_return_val_if_fail (EOG_IS_JOB (job), 0);
	eog_job_manager_init ();
	
	g_mutex_lock (mutex);
	
	if (eog_job_get_priority (job) == EOG_JOB_PRIORITY_NORMAL)
		g_queue_push_tail (job_list, g_object_ref (job));
	else
		g_queue_push_head (job_list, g_object_ref (job));

	if (n_threads < MAX_THREADS) {
		guint thread_id;

		n_threads++;

		for (thread_id = 0; thread_id < MAX_THREADS; thread_id++) {
			if (threads[thread_id].thread == NULL) break;
		}

		if (thread_id < MAX_THREADS) {
			threads[thread_id].thread =
				g_thread_create (thread_start_func, GUINT_TO_POINTER (thread_id), FALSE, NULL);
			threads[thread_id].job = NULL;
		}
	}
	else {
		g_cond_broadcast (cond);
	}
	g_mutex_unlock (mutex);

	return eog_job_get_id (job);
}

static gint
compare_job_id (gconstpointer a, gconstpointer b)
{
	EogJob *job;
	guint job_id;

	if (EOG_IS_JOB (a)) {
		job = EOG_JOB (a);
		job_id = GPOINTER_TO_UINT (b);
	}
	else {
		job = EOG_JOB (b);
		job_id = GPOINTER_TO_UINT (a);
	}

	return (job_id - eog_job_get_id (job));
}

/* this function doesn't lock the mutex! */
static EogJob*
eog_job_manager_get_job_private (guint id, GList **link)
{
	guint thread_id;
	EogJob *job = NULL;

	if (link != NULL) {
		*link = NULL;
	}

	/* first see if the job we are looking for is under the ones currently
	 * processed 
	 */
	for (thread_id = 0; thread_id < MAX_THREADS; thread_id++) {
		if (threads[thread_id].job != NULL && 
		    eog_job_get_id (threads[thread_id].job) == id) 
		{
			job = threads[thread_id].job;
			break;
		}
	}
	
	if (job == NULL) {
		GList *it;
		
		it = g_queue_find_custom (job_list,
					  GUINT_TO_POINTER (id),
					  (GCompareFunc) compare_job_id);

		if (it != NULL) {
			job = EOG_JOB (it->data);
		}

		if (link != NULL) {
			*link = it;
		}
	}

	return job;
}

/* called from main thread */
gboolean
eog_job_manager_cancel_job (guint id)
{
	EogJob *job;
	GList *link = NULL;
	gboolean success = FALSE;
	gboolean call_finished = FALSE;

	eog_job_manager_init ();

	g_mutex_lock (mutex);
	job = eog_job_manager_get_job_private (id, &link);
	if (link != NULL) {
		g_queue_delete_link (job_list, link);
	}
	g_mutex_unlock (mutex);

	if (job != NULL) {
		call_finished = (eog_job_get_status (job) == EOG_JOB_STATUS_WAITING);

		/* we found the job, now cancel it */
		/* This should stop the user specified action
		 * function and leads to the return of the 
		 * eog_job_call_action (job) in thread_start_func.
		 */
		success = eog_job_call_canceled (job);

		/* if it's in the waiting queue (status == WAITING)
		 * call the finished function also 
		 */
		if (success && call_finished) {
			g_idle_add (job_finished_cb, job);
		}
	}

	return success;
}

/* called from main thread */
EogJob*
eog_job_manager_get_job (guint id)
{
	EogJob *job;

	eog_job_manager_init ();

	g_mutex_lock (mutex);
	job = eog_job_manager_get_job_private (id, NULL);
	g_mutex_unlock (mutex);
	
	if (job != NULL) 
		g_object_ref (job);
	
	return job;
}

static void
cancel_each_job (gpointer data, gpointer user_data)
{
	EogJob *job = EOG_JOB (data);
	gboolean success;

	g_assert (eog_job_get_status (job) == EOG_JOB_STATUS_WAITING);

	success = eog_job_call_canceled (job);
	if (success) {
		g_idle_add (job_finished_cb, job);
	}
	else {
		g_object_unref (G_OBJECT (job));
	}
}

/* called from main thread */
void      
eog_job_manager_cancel_all_jobs (void)
{
	guint thread_id;

	eog_job_manager_init ();

	g_mutex_lock (mutex);

	/* stop all currently running threads */
	for (thread_id = 0; thread_id < MAX_THREADS; thread_id++) {
		if (threads[thread_id].job != NULL) {
			eog_job_call_canceled (threads[thread_id].job);
		}
	}

	/* remove all jobs from waiting list and free list structure
	 * in one run */
	g_queue_foreach (job_list, (GFunc) cancel_each_job, NULL);
	g_queue_free (job_list);
	job_list = g_queue_new ();
		
	g_mutex_unlock (mutex);
}
