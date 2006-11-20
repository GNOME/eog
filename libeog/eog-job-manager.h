#ifndef _EOG_JOB_MANAGER_H_
#define _EOG_JOB_MANAGER_H_

#include "eog-job.h"

G_BEGIN_DECLS

guint     eog_job_manager_add        (EogJob *job);
EogJob*   eog_job_manager_get_job    (guint id);
gboolean  eog_job_manager_cancel_job (guint id);
void      eog_job_manager_cancel_all_jobs (void);

G_END_DECLS

#endif /* _EOG_JOB_MANAGER_H_ */
