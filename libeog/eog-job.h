#ifndef _EOG_JOB_H_
#define _EOG_JOB_H_

#include <glib-object.h>


G_BEGIN_DECLS

#define EOG_TYPE_JOB            (eog_job_get_type ())
#define EOG_JOB(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_JOB, EogJob))
#define EOG_JOB_CLASS(k)   (G_TYPE_CHECK_CLASS_CAST((k), EOG_TYPE_JOB, EogJobClass))
#define EOG_IS_JOB(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_JOB))
#define EOG_IS_JOB_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), EOG_TYPE_JOB))
#define EOG_JOB_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), EOG_TYPE_JOB, EogJobClass))

typedef struct _EogJob EogJob;
typedef struct _EogJobClass EogJobClass;
typedef struct _EogJobPrivate EogJobPrivate;

struct _EogJob {
	GObject parent;

	EogJobPrivate *priv;
};

struct _EogJobClass {
	GObjectClass parent_klass;
};

typedef enum {
	EOG_JOB_STATUS_ERROR,
	EOG_JOB_STATUS_WAITING,
	EOG_JOB_STATUS_RUNNING,
	EOG_JOB_STATUS_FINISHED,
	EOG_JOB_STATUS_CANCELED
} EogJobStatus;

typedef enum {
	EOG_JOB_PRIORITY_NORMAL = 0,
	EOG_JOB_PRIORITY_HIGH   = 1
} EogJobPriority;

typedef void  ((* EogJobActionFunc)    (EogJob *job, gpointer data, GError **error));
typedef void  ((* EogJobFinishedFunc)  (EogJob *job, gpointer data, GError *error));
typedef void  ((* EogJobCancelFunc)    (EogJob *job, gpointer data));
typedef void  ((* EogJobFreeDataFunc)  (gpointer data));
typedef void  ((* EogJobProgressFunc)  (EogJob *job, gpointer data, float progress));

/* Public API */
GType               eog_job_get_type                       (void) G_GNUC_CONST;

EogJob*             eog_job_new (GObject *data_obj, 
				 EogJobActionFunc af,
				 EogJobFinishedFunc ff,
				 EogJobCancelFunc cf,
				 EogJobProgressFunc pf);

EogJob*             eog_job_new_full (gpointer data, 
				      EogJobActionFunc af,
				      EogJobFinishedFunc ff,
				      EogJobCancelFunc cf,
				      EogJobProgressFunc pf,
				      EogJobFreeDataFunc df);
void                eog_job_part_finished (EogJob *job);
EogJobStatus        eog_job_get_status   (EogJob *job);
guint               eog_job_get_id       (EogJob *job);
gboolean            eog_job_get_success  (EogJob *job);
void                eog_job_set_progress (EogJob *job, float progress);
EogJobPriority      eog_job_get_priority (EogJob *job);

/* API only used by the EogJobManager */
void                eog_job_call_action   (EogJob *job);
void                eog_job_call_finished (EogJob *job);
gboolean            eog_job_call_canceled (EogJob *job);


G_END_DECLS

#endif /* _EOG_JOB_H_ */
