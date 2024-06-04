
#include <mutex>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "conf.h"
#include "rpc.h"
#include "debug.h"
#include "fault.h"
#include "net.h"
#include "lock.h"

using std::mutex;

uint64_t* fault_signal;
mutex integrity_mtx;

int check_error(char caller, lock_id lock, task_id task) {

  // Detect the fault signal.
  auto code = *fault_signal;
  if (code == 0) return 0;

  // If another thread is performing the integrity check,
  // we do not need to perform it again.
  integrity_mtx.lock();
  if (*fault_signal) {
    LOG("detected error: %ld", code);
    integrity_check(code);
    *fault_signal = 0;
  }
  integrity_mtx.unlock();

  // For waiter, need to check whether it needs to abort.
  if (caller == CALLER_WAIT_FOR_GRANT) {
    check_waiter_req req = {
      .lid = lock, .waiter = LOCK_HOLDER(LOCALHOST_ID, task)};
    check_waiter_reply* reply = NULL;

    // Keep retrying until getting a valid result.
    while (!reply || reply->exist == 2) {
      sleep(1);
      reply = (check_waiter_reply *)rpc(RPC_CHECK_WAITER, 
        (char *)&req, sizeof(req), NULL);
    }

    // If the waiter does not exist in wqueue, return 1 to abort it.
    if (reply->exist == 1) return 0;
    else return 1;
  }

  return 0;
}

void fault_detector_setup() {
  int fd = open(FILE_FAULT_SIGNAL, O_RDWR | O_CREAT, 0777);
  ASSERT(ftruncate(fd, 4096) == 0);
  fault_signal = (uint64_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
    MAP_SHARED, fd, 0);
}