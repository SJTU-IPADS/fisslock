#ifndef TPCCLOCKGEN_H
#define TPCCLOCKGEN_H

#include <stdlib.h>
#include <iostream>
#include <vector>
#include <map>
#include <cstddef>

using namespace std;

// Utility macros.
#define GEN_OUTPUT(lr) do {\
  printf("%u,%d,%d,%d,%d\n", lr->user_id, lr->task, lr->txn_type,\
    lr->obj_index, lr->lock_type);\
} while (0)

#define GEN_REQUEST(tid, wid, tt, lt, lid) do {\
  LockRequest *lr = (LockRequest*)malloc(sizeof(LockRequest));\
  lr->user_id = tid;\
  lr->lm_id = wid;\
  lr->txn_type = tt;\
  lr->lock_type = lt;\
  lr->obj_index = lid;\
  lr->task = LOCK;\
  requests.push_back(lr);\
} while (0)

#define URAND(range) (rand_r(&tpcc_rand_seed) % range)
#define NURAND(A, range) ((URAND(A) | (URAND(range))) % range)

// Configurable factors.
static const int MACHINE_NUM = 8;
static const int WPM = 1200; /* Warehouse per machine */

// Transaction types.
#define NEW_ORDER    1
#define PAYMENT      2
#define ORDER_STATUS 3
#define DELIVERY     4
#define STOCK_LEVEL  5

// Object numbers in TPC-C.
#define NUM_WAREHOUSE  1
#define NUM_DISTRICT   10
#define NUM_CUSTOMER   30000
#define NUM_ORDER      30000
#define NUM_HISTORY    30000
#define NUM_NEW_ORDER  30000
#define NUM_ORDER_LINE 300000
#define NUM_STOCK      100000
#define NUM_ITEM       100000

#define CPD (NUM_CUSTOMER / NUM_DISTRICT) /* Customer per district */

// Object lock numbers in TPC-C.
// 
// Warehouses and districts use traditional per-row lock.
#define NUM_WAREHOUSE_LOCK  1
#define NUM_DISTRICT_LOCK   10
// For tables that may be inserted into by txns, we only assign one 
// logic lock per table to protect it. 
#define NUM_ORDER_LOCK      10
#define NUM_NEW_ORDER_LOCK  10
#define NUM_HISTORY_LOCK    10
#define NUM_ORDER_LINE_LOCK 10
// To compress the lock space, for other tables we lock row partitions
// instead of rows.
#define NUM_ITEM_LOCK       10
#define NUM_CUSTOMER_LOCK   30
#define NUM_STOCK_LOCK      30

#define LOCK_PER_WAREHOUSE (NUM_WAREHOUSE_LOCK + NUM_DISTRICT_LOCK + \
  NUM_CUSTOMER_LOCK + NUM_ORDER_LINE_LOCK + NUM_HISTORY_LOCK + \
  NUM_NEW_ORDER_LOCK + NUM_STOCK_LOCK + NUM_ITEM_LOCK + NUM_ORDER_LOCK)

// Lock types.
#define SHARED    1
#define EXCLUSIVE 2
#define LOCK      0
#define UNLOCK    1

typedef struct {
  int      seq_no;    // sequence no (id)
  uint32_t user_id;
  int      task;      // lock, unlock
  uint32_t lm_id;
  int      obj_index; // lock id
  int      lock_type; // shared, exclusive
  int      txn_type;
} LockRequest;

class TPCCLockGen {
  public:
    TPCCLockGen(int machine_id);
    ~TPCCLockGen();
    void Generate(int txn_id, std::vector<LockRequest*>& requests);

    int WAREHOUSE_START_IDX[WPM * MACHINE_NUM];
    int DISTRICT_START_IDX[WPM * MACHINE_NUM];
    int CUSTOMER_START_IDX[WPM * MACHINE_NUM];
    int ORDER_START_IDX[WPM * MACHINE_NUM];
    int HISTORY_START_IDX[WPM * MACHINE_NUM];
    int NEW_ORDER_START_IDX[WPM * MACHINE_NUM];
    int ORDER_LINE_START_IDX[WPM * MACHINE_NUM];
    int STOCK_START_IDX[WPM * MACHINE_NUM];
    int ITEM_START_IDX[WPM * MACHINE_NUM];

    static const int NUM_ORDER_LINE_PER_ORDER  = 10;
    // static const int NUM_ORDER_LINE_PER_STOCK  = 3;
    // static const int NUM_HISTORY_PER_CUSTOMER  = 1;

    int REMOTE_WID(int mid) {
      int _smid = URAND(MACHINE_NUM);
      while (_smid == mid) _smid = URAND(MACHINE_NUM);
      return _smid * WPM + URAND(WPM);
    }

  private:
    void GenerateScanCustomer(std::vector<LockRequest*>& requests);
    void GenerateNewOrder(std::vector<LockRequest*>& requests);
    void GeneratePayment(std::vector<LockRequest*>& requests);
    void GenerateOrderStatus(std::vector<LockRequest*>& requests);
    void GenerateDelivery(std::vector<LockRequest*>& requests);
    void GenerateStockLevel(std::vector<LockRequest*>& requests);

    int  tpcc_machine_id;
    int* tpcc_txn_mix;
    int  tpcc_txn_id;

    unsigned int tpcc_rand_seed;
};

#endif
