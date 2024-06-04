
#ifndef __FISSLOCK_FAULT_H
#define __FISSLOCK_FAULT_H

#define SERVER_FAILED(code, mid) (!(code & (1 << mid)))

#define CALLER_ACQUIRE_OR_RELEASE 0x1
#define CALLER_WAIT_FOR_GRANT     0x2

#ifdef __cplusplus
extern "C" {
#endif

void fault_detector_setup();
int check_error(char caller, lock_id lock, task_id task);

#ifdef __cplusplus
}
#endif

#endif
