#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include "eog-job-manager.h"
#include "eog-image.h"

static guint req_data = EOG_IMAGE_DATA_ALL;

static void
image_load_sync (EogJob *job, gpointer data, GError **error)
{
	EogTransform *trans;

	g_assert (!eog_image_has_data (EOG_IMAGE (data), req_data));

	trans = eog_transform_rotate_new (90);

	eog_image_transform (EOG_IMAGE (data), trans, job);

	eog_image_load (EOG_IMAGE (data),
			req_data,
			job,
			error);
}

static void
image_finished_cb (EogJob *job, gpointer data, GError *error)
{
	EogImage *img;

	img = EOG_IMAGE (data);

	switch (eog_job_get_status (job)) {
	case EOG_JOB_STATUS_FINISHED:
		if (eog_job_get_success (job)) {
			g_assert (eog_image_has_data (img, req_data));
			g_print ("Image loaded successfully.\n");
			g_assert (error == NULL);
		}
		else {
			g_print ("Image loading failed: %s.\n",
				 error == NULL ? "???" : error->message);
		}
		break;
	case EOG_JOB_STATUS_CANCELED:
		g_print ("Image loading canceled.\n");
		break;
	default:
		g_print ("Invalid job status after finishing.\n");
	} 

	g_object_unref (G_OBJECT (img));

	gtk_main_quit ();
}

static void
image_cancel_cb (EogJob *job, gpointer data)
{
	eog_image_cancel_load (EOG_IMAGE (data));
}

static void
image_progress_cb (EogJob *job, gpointer data, float progress)
{
	static gboolean first_call = TRUE;

	g_print ("Progress %3.2f\n", progress * 100.0);

	if (first_call) {
		eog_job_manager_cancel_job (eog_job_get_id (job));
		first_call = FALSE;
	}
}

static gboolean
test_image_load_idle (gpointer d)
{
	char *txt_uri;
	EogImage *image;
	EogJob *job;

	txt_uri = (char*) d;

	image = eog_image_new  (txt_uri);
	
	job = eog_job_new ((gpointer) image,
			   image_load_sync,
			   image_finished_cb,
			   image_cancel_cb,
			   image_progress_cb);

	g_object_set (G_OBJECT (job), "progress-threshold", 0.6, NULL);

	eog_job_manager_add (job);

	return FALSE;
}

int
main (int argc, char **argv) 
{
	gtk_init (&argc, &argv);
	g_thread_init (NULL);
	gnome_vfs_init ();

	if (argc > 1) {
		g_idle_add (test_image_load_idle, argv[1]);

		gtk_main ();
	}
	else {
		g_print ("Usage: %s <file>\n", argv[0]);
	}

	return 0;
}

