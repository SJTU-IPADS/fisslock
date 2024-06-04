
#ifndef _SWITCH_BFRT_H
#define _SWITCH_BFRT_H

/********************************************************* 
 * String literals in the P4 program.                    *
 *********************************************************/

#define P4_PROGRAM_NAME         "fisslock_decider"
#define P4_INGRESS_NAME         "IngressPipe"
#define P4_HEADERS_NAME         "hdr"

#define P4_TABLE(name)          P4_INGRESS_NAME "." name
#define P4_ACTION(name)         P4_INGRESS_NAME "." name
#define P4_REGISTER_DATA(name)  P4_INGRESS_NAME "." name ".f1"

/********************************************************* 
 * Machines in the cluster.                              *
 *********************************************************/

typedef struct {
  char* hostname;
  uint32_t ip_addr;
  uint64_t mac_addr;
  uint32_t port;
} machine_info;

static const machine_info connected_machines[] = {
#include "cluster.h"
};

#define MACHINE_NUM (sizeof(connected_machines) / sizeof(machine_info))
#define MID(i) ((i) + 1)

void driver_init();

// void lock_arr_add(lid_t lock, uint64_t data);
// void lock_arr_mod(lid_t lock, uint64_t data);
// void lock_arr_sync();

#endif