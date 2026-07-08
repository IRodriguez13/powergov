#ifndef POWERGOV_SESSION_IDLE_H
#define POWERGOV_SESSION_IDLE_H

/* Returns 0 if read; *idle_out is 1 idle, 0 active. -1 if unknown. */
int powergov_session_idle_poll(int *idle_out);

#endif
