#include "eog-job-manager.h"

typedef struct {
	GThread *thread;
	EogJob  *job;
} ThreadData;

#define MAX_THREADS  3
#define DEBUG_JOB_MANAGER 1
GStaticMutex  stat_mutex  = G_STATIC_MUTEX_INIT;
GSList       *job_list    = NULL;
guint         n_threads   = 0;
ThreadData    threads[MAX_THREADS];
gboolean      init = FALSE;

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
	g_print ("Starting thread with id %i.\n", thread_id);
#endif

	while (TRUE) {
		job = NULL;
		
		g_static_mutex_lock (&stat_mutex);
		if (job_list != NULL) {
			g_assert (EOG_IS_JOB (job_list->data));
			job = EOG_JOB (job_list->data);
			job_list = g_slist_delete_link (job_list, job_list);
			threads[thread_id].job = job;
		}
		g_static_mutex_unlock (&stat_mutex);

		if (job == NULL)
			break;

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

#if DEBUG_JOB_MANAGER
	g_print ("Stopping thread with id %i.\n", thread_id);
#endif
	/* clean up and remove thread */
	g_static_mutex_lock (&stat_mutex);
	n_threads--;
	g_assert (n_threads >= 0);
	threads[thread_id].thread = NULL;
	threads[thread_id].job = NULL;
	g_static_mutex_unlock (&stat_mutex);
	
	return NULL;
}

/* called from main thread */
guint     
eog_job_manager_add (EogJob *job)
{
	g_return_val_if_fail (EOG_IS_JOB (job), 0);
	eog_job_manager_init ();
	
	g_static_mutex_lock (&stat_mutex);
	
	job_list = g_slist_append (job_list, g_object_ref (job));

	if (n_threads < MAX_THREADS) {
		n_threads++;
		guint thread_id;

		for (thread_id = 0; thread_id < MAX_THREADS; thread_id++) {
			if (threads[thread_id].thread == NULL) break;
		}

		if (thread_id < MAX_THREADS) {
			threads[thread_id].thread =
				g_thread_create (thread_start_func, GUINT_TO_POINTER (thread_id), TRUE, NULL);
			threads[thread_id].job = NULL;
		}
	}
	g_static_mutex_unlock (&stat_mutex);

	return eog_job_get_id (job);
}

/* this function doesn't lock the mutex! */
static EogJob*
eog_job_manager_get_job_private (guint id, GSList **link)
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
		GSList *it;
		/* try to find the job in the list of waiting jobs */
		for (it = job_list; it != NULL; it = it->next) {
			EogJob *j;

			j = EOG_JOB (it->data);
			if (eog_job_get_id (j) == id) {
				job = j;
				break;
			}
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
	GSList *link = NULL;
	gboolean success = FALSE;
	gboolean call_finished = FALSE;

	eog_job_manager_init ();

	g_static_mutex_lock (&stat_mutex);
	job = eog_job_manager_get_job_private (id, &link);
	if (link != NULL) {
		job_list = g_slist_delete_link (job_list, link);
	}
	g_static_mutex_unlock (&stat_mutex);

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

	g_static_mutex_lock (&stat_mutex);
	job = eog_job_manager_get_job_private (id, NULL);
	g_static_mutex_unlock (&stat_mutex);
	
	return job;
}

/* called from main thread */
void      
eog_job_manager_cancel_all_jobs (void)
{
	EogJob *job;
	guint thread_id;
	GSList *it;
	gboolean success;

	eog_job_manager_init ();

	g_static_mutex_lock (&stat_mutex);

	/* stop all currently running threads */
	for (thread_id = 0; thread_id < MAX_THREADS; thread_id++) {
		if (threads[thread_id].job != NULL) {
			eog_job_call_canceled (threads[thread_id].job);
		}
	}

	/* remove all jobs from waiting list and free list structure
	 * in one run */
	it = job_list;
	while (it != NULL) {
		job = EOG_JOB (it->data);
		g_assert (eog_job_get_status (job) == EOG_JOB_STATUS_WAITING);

		success = eog_job_call_canceled (job);
		if (success) {
			g_idle_add (job_finished_cb, job);
		}
		else {
			g_object_unref (G_OBJECT (job));
		}
		
		it = g_slist_delete_link (it, it);
	}
	job_list = NULL;
		
	g_static_mutex_unlock (&stat_mutex);
}
