#ifndef __PARLOCK_SERVER_H
#define __PARLOCK_SERVER_H

#include "debug.h"
#include "lock.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    This function will free `lk_hd`
*/
void server_handle_exclusive_lock(uint32_t lcore_id, lock_post_header* lk_hd);
/*
    This function will free `lk_hd`
*/
void server_handle_shared_lock(uint32_t lcore_id, lock_post_header* lk_hd);
/*
    This function will free `lk_hd`
*/
void server_handle_exclusive_unlock(uint32_t lcore_id, lock_post_header* lk_hd);
/*
    This function will free `lk_hd`
*/
void server_handle_shared_unlock(uint32_t lcore_id, lock_post_header* lk_hd);

#ifdef __cplusplus
}
#endif


#endif
