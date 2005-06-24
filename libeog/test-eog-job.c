#include <unistd.h>
#include <gtk/gtk.h>
#include "eog-job-manager.h"

typedef struct {
	gboolean cancel;
} TestData;

static int n_jobs = 0;

static void 
action_cb (EogJob *job, gpointer data, GError **error)
{
	TestData *td = (TestData*) data;
	int i;
	int n_runs;

	g_print ("Job %.3i: action ...\n", eog_job_get_id (job));

	n_runs = g_random_int_range (2, 5);

	for (i = 0; i < n_runs; i++) {
		g_usleep (1000000);
		eog_job_set_progress (job, (float) i / (float) (n_runs-1));
		if (td->cancel) {
			break;
		}
	}

	g_print ("Job %.3i: action end ...\n", eog_job_get_id (job));
}

static void
finished_cb (EogJob *job, gpointer data, GError *error)
{
	if (eog_job_get_status (job) == EOG_JOB_STATUS_CANCELED) {
		g_print ("Job %.3i: finished but canceled.\n", eog_job_get_id (job));
	}
	else {
		g_print ("Job %.3i: finished - %s\n", eog_job_get_id (job),
			 eog_job_get_success (job) ? "success" : "failed");
	}

	--n_jobs;

	g_print ("n_jobs: %i\n", n_jobs);
	if (n_jobs == 0)
		gtk_main_quit ();
}

static void
cancel_cb (EogJob *job, gpointer data)
{
	g_print ("Job %.3i: cancel job ...\n", eog_job_get_id (job));
	((TestData*)data)->cancel = TRUE;
}

static void
progress_cb (EogJob *job, gpointer data, float progress)
{
	g_print ("Job %.3i: %.0f%%\n", eog_job_get_id (job), (progress * 100.0));
}

static void
free_data_cb (gpointer data)
{
	g_free (data);
}

#if 0
static gboolean
cancel_job_one (gpointer data)
{
	gboolean result;

	g_print ("Job %.3i: canceling job from manager.\n", 1);
	result = eog_job_manager_cancel_job (1);
	g_print ("Job %.3i: cancel %s\n", 1, result ? "successfull" : "failed");
	return FALSE;
}
#endif

static gboolean
cancel_all_jobs (gpointer data)
{
	eog_job_manager_cancel_all_jobs ();

	/* Let the main loop finish all the idle loop
	 * handler for finishing the jobs 
	 */
	while (gtk_events_pending ())
		gtk_main_iteration ();
	
	return FALSE;
}

static gboolean
test_jobs (gpointer d)
{
	int i;
	EogJob *job;
#if 0
	GRand *rand;
#endif
	for (i = 0; i < 20; i++) {
		TestData *data = g_new0 (TestData, 1);
		data->cancel = FALSE;

		job = eog_job_new_full (data,
					action_cb, 
					finished_cb,
					cancel_cb,
					progress_cb,
					free_data_cb);

		eog_job_manager_add (job);

		g_object_unref (G_OBJECT (job));

		n_jobs++;
	}

	g_timeout_add (4000, cancel_all_jobs, NULL);

#if 0
	g_timeout_add (8000, cancel_job_one, NULL);

	rand = g_rand_new_with_seed (2023424);

	for (i = 0; i < 7; i++) {
		int id = g_rand_int_range (rand, 2, 20);
		gboolean result;
		
		g_print ("Job %.3i: canceling job from manager.\n", id);
		result = eog_job_manager_cancel_job (id);
		g_print ("Job %.3i: cancel %s\n", id, 
			 result ? "successfull" : "failed");
	}

	g_rand_free (rand);
#endif

	return FALSE;
}

int
main (int argc, char **argv) 
{

	gtk_init (&argc, &argv);
	g_thread_init (NULL);

	g_idle_add (test_jobs, NULL);


	gtk_main ();

	return 0;
}
