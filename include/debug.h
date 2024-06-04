
#ifndef __FISSLOCK_DEBUG_H
#define __FISSLOCK_DEBUG_H

#include <stdlib.h>
#include <stdio.h>

#include "conf.h"
#include "types.h"
#include "statistics.h"

// #define FISSLOCK_DEBUG

typedef enum {
  ERR_NOLOCK = 1,
  ERR_LOCK_DOUBLE_FREE,
  ERR_INVALID_OP,
  ERR_INVALID_FREE_ADDR,
} error_code;

#define LOG(fmt, ...) do {\
  fprintf(stderr, "[host%u][Info]" fmt "\n", LOCALHOST_ID, ##__VA_ARGS__);\
  fflush(stderr);\
} while (0)

#define EXCEPTION(fmt, ...) do {\
  fprintf(stderr, "[host%u][Exception] <%s:%d> " fmt "\n", \
    LOCALHOST_ID, __FILE__, __LINE__, ##__VA_ARGS__);\
  exit(1);\
} while (0)

#define ERROR(fmt, ...) do {\
  fprintf(stderr, "[host%u][Error] " fmt "\n", LOCALHOST_ID, ##__VA_ARGS__);\
  fflush(stderr);\
  fflush(stdout);\
  exit(1);\
} while (0)

#ifdef FISSLOCK_DEBUG

#define ASSERT(cond) do {\
  if (!(cond)) {\
    fprintf(stderr, "[host%d][Assertion failed] <%s:%d> %s\n",\
      LOCALHOST_ID, __FILE__, __LINE__, #cond);\
    exit(1);\
  }\
} while (0)

#define ASSERT_MSG(cond, msg, ...) do {\
  if (!(cond)) {\
    fprintf(stderr, "[host%d][Assertion failed] <%s:%d> " msg "\n",\
      LOCALHOST_ID, __FILE__, __LINE__, ##__VA_ARGS__);\
    exit(1);\
  }\
} while (0)

#define TODO(msg) do {\
  printf("[TODO] <%s:%d> " msg "\n", __FILE__, __LINE__);\
} while (0)

#define DEBUG(fmt, ...) do {\
  fprintf(stderr, "[host%u][%ld][Debug]" fmt "\n", \
    LOCALHOST_ID, timer_now() % 1000000, ##__VA_ARGS__);\
  fflush(stderr);\
} while (0)

#else

#define ASSERT(cond) (cond)
#define ASSERT_MSG(cond, msg, ...) (cond)
#define TODO(msg) (0)
#define DEBUG(fmt, ...) (0)

#endif

#endif