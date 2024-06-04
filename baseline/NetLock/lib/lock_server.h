#ifndef __NETLOCK_LOCK_SERVER_H
#define __NETLOCK_LOCK_SERVER_H

#include "request.h"
#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int *len_in_switch;

/*
    This function will free `node`
*/
void process_primary_backup(uint32_t lcore_id, lock_queue_node *node);
/*
    This funtction will free 'node
*/
void process_secondary_backup(uint32_t lcore_id, lock_queue_node *node);

static void process_primary_acquire(uint32_t lcore_id, lock_queue_node *node);
static void process_primary_release(uint32_t lcore_id, lock_queue_node *node);
static void process_secondary_acquire(uint32_t lcore_id, lock_queue_node *node);
static void process_secondary_release(uint32_t lcore_id, lock_queue_node *node);


#ifdef __cplusplus
}
#endif


#endif