#ifndef POWERGOV_LID_STATE_H
#define POWERGOV_LID_STATE_H

/* Returns 0 if lid state read; *closed_out is 1 closed, 0 open. -1 if unknown. */
int powergov_lid_poll(int *closed_out);

#endif
