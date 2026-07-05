#ifndef POWERGOV_TLP_COMPAT_H
#define POWERGOV_TLP_COMPAT_H

#include "../include/powergov/types.h"

/* True when TLP is installed and its service (or runtime) is active. */
int tlp_active(void);

/* Device PM layers deferred to TLP when it is active. */
int tlp_defers_feature(powergov_feature_id_t id);

#endif
