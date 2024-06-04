#include <unistd.h>
#include "tpcc_lock_gen.h"

static int ORDER_NUM = 0;
static int NEW_ORDER_NUM = 0;
static int HISTORY_NUM = 0;
static int ORDER_LINE_NUM = 0;

TPCCLockGen::TPCCLockGen(int machine_id) {
  tpcc_machine_id = machine_id;

  // Configure the proportion of each type of txn in
  // the standard mix.
  tpcc_txn_mix = new int[5];
  tpcc_txn_mix[0] = 45;   // NewOrder 45
  tpcc_txn_mix[1] = 88;   // Payment 43
  tpcc_txn_mix[2] = 92;   // OrderStatus 4
  tpcc_txn_mix[3] = 96;   // Delivery 4
  tpcc_txn_mix[4] = 100;  // StockLevel 4

  // The items in the database.
  // items_ = new int[NUM_ORDER_LINE_PER_ORDER];

  // Spawn a seed for following random number generation.
  struct timespec now_time = {0, 0};
  clock_gettime(CLOCK_REALTIME, &now_time);
  tpcc_rand_seed = now_time.tv_nsec + getpid() + (uintptr_t) this;

  // Find the starting lock ID in each warehouse.
  for (int i = 0; i < WPM * MACHINE_NUM; i++) {
    WAREHOUSE_START_IDX[i] = i * LOCK_PER_WAREHOUSE;
    DISTRICT_START_IDX[i] = WAREHOUSE_START_IDX[i] + NUM_WAREHOUSE_LOCK;
    CUSTOMER_START_IDX[i] = DISTRICT_START_IDX[i] + NUM_DISTRICT_LOCK;
    ORDER_START_IDX[i] = CUSTOMER_START_IDX[i] + NUM_CUSTOMER_LOCK;
    HISTORY_START_IDX[i] = ORDER_START_IDX[i] + NUM_ORDER_LOCK;
    NEW_ORDER_START_IDX[i] = HISTORY_START_IDX[i] + NUM_HISTORY_LOCK;
    ORDER_LINE_START_IDX[i] = NEW_ORDER_START_IDX[i] + NUM_NEW_ORDER_LOCK;
    STOCK_START_IDX[i] = ORDER_LINE_START_IDX[i] + NUM_ORDER_LINE_LOCK;
    ITEM_START_IDX[i] = STOCK_START_IDX[i] + NUM_STOCK_LOCK;
  }
}

TPCCLockGen::~TPCCLockGen() { 
  delete[] tpcc_txn_mix; 
}

void TPCCLockGen::Generate(int txn_id, std::vector<LockRequest*>& requests) {
  tpcc_txn_id = txn_id;

  int val = rand_r(&tpcc_rand_seed) % 100;
  if (val < tpcc_txn_mix[0]) {
    GenerateNewOrder(requests); // 17
  } else if (val < tpcc_txn_mix[1]) {
    GeneratePayment(requests); // 5.2
  } else if (val < tpcc_txn_mix[2]) {
    GenerateOrderStatus(requests); // 12.8
  } else if (val < tpcc_txn_mix[3]) {
    GenerateDelivery(requests); // 13
  } else if (val < tpcc_txn_mix[4]) {
    GenerateStockLevel(requests); // 21
  } 

  return;
}

/* New Order Transaction: TPCC Spec Page 28 of 130
 */
void TPCCLockGen::GenerateNewOrder(std::vector<LockRequest*>& requests) {
  int tid = tpcc_txn_id;
  int wid = tpcc_machine_id * WPM + URAND(WPM);

  // "getWarehouseTaxRate"
  GEN_REQUEST(tid, wid, NEW_ORDER, SHARED, WAREHOUSE_START_IDX[wid]);

  // "getDistrict"
  int did = URAND(NUM_DISTRICT_LOCK);
  GEN_REQUEST(tid, wid, NEW_ORDER, EXCLUSIVE, DISTRICT_START_IDX[wid] + did);

  // "getCustomer"
  int cid = (NURAND(1024, CPD) + did * CPD) / /* The selected row ID */
            (NUM_CUSTOMER / NUM_CUSTOMER_LOCK); /* The number of regions */
  GEN_REQUEST(tid, wid, NEW_ORDER, SHARED, CUSTOMER_START_IDX[wid] + cid);

  // "createOrder"
  int olk = (ORDER_NUM++) % NUM_ORDER_LOCK;
  GEN_REQUEST(tid, wid, NEW_ORDER, EXCLUSIVE, ORDER_START_IDX[wid] + olk);

  // "createNewOrder"
  int nolk = (NEW_ORDER_NUM++) % NUM_NEW_ORDER_LOCK;
  GEN_REQUEST(tid, wid, NEW_ORDER, EXCLUSIVE, NEW_ORDER_START_IDX[wid] + nolk);

  // "getItemInfo"
  // "getStockInfo"
  // "createOrderLine"
  map<int, bool> iid_used, sid_used;
  for (int i = 0; i < NUM_ORDER_LINE_PER_ORDER; ++i) {
    int iid = NURAND(8192, NUM_ITEM) % NUM_ITEM_LOCK;
    while (iid_used[iid]) iid = NURAND(8192, NUM_ITEM) % NUM_ITEM_LOCK;
    iid_used[iid] = true;

    // Items are shared, no need to lock.
    // if (URAND(100) == 1) {
    //   int swid = REMOTE_WID(tpcc_machine_id);
    //   GEN_REQUEST(tid, wid, NEW_ORDER, SHARED, ITEM_START_IDX[swid] + iid);
    // } else {
    //   GEN_REQUEST(tid, wid, NEW_ORDER, SHARED, ITEM_START_IDX[wid] + iid);
    // }

    int sid = URAND(NUM_STOCK) % NUM_STOCK_LOCK;
    if (!sid_used[sid]) {
      sid_used[sid] = true;
      GEN_REQUEST(tid, wid, NEW_ORDER, EXCLUSIVE, STOCK_START_IDX[wid] + sid);
    }
  }

  int ollk = (ORDER_LINE_NUM++) % NUM_ORDER_LINE_LOCK;
  GEN_REQUEST(tid, wid, NEW_ORDER, EXCLUSIVE, ORDER_LINE_START_IDX[wid] + ollk);
}

/* Payment Transaction: TPCC Spec Page 33 of 130
 */
void TPCCLockGen::GeneratePayment(std::vector<LockRequest*>& requests) {
  int tid = tpcc_txn_id;
  int wid = tpcc_machine_id * WPM + URAND(WPM);
  int x = URAND(100), y = URAND(100);

  // "getWarehouse"
  GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, WAREHOUSE_START_IDX[wid]);

  // "getDistrict"
  int did = URAND(NUM_DISTRICT_LOCK);
  GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, DISTRICT_START_IDX[wid] + did);

  // "getCustomersByLastName"
  // 
  // There are 1000 types of customer last names. Each district has 3000
  // customers. Hence, select by last name should hit 3 customers in average.
  if (y < 60) {
    int cid = NURAND(256, 1000) % NUM_CUSTOMER_LOCK;
    if (x < 85) {
      GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, CUSTOMER_START_IDX[wid] + cid);
    } else {
      int swid = REMOTE_WID(tpcc_machine_id);
      GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, CUSTOMER_START_IDX[swid] + cid);
    }

  // "getCustomerByCustomerId"
  } else {
    int cid = NURAND(1024, 3000) % NUM_CUSTOMER_LOCK;
    if (x < 85) {
      GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, CUSTOMER_START_IDX[wid] + cid);
    } else {
      int swid = REMOTE_WID(tpcc_machine_id);
      GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, CUSTOMER_START_IDX[swid] + cid);
    }
  }

  // "insertHistory"
  int hlk = (HISTORY_NUM++) % NUM_HISTORY_LOCK;
  GEN_REQUEST(tid, wid, PAYMENT, EXCLUSIVE, HISTORY_START_IDX[wid] + hlk);
}

/* Order Status Transaction: TPCC Spec Page 37 of 130
 */
void TPCCLockGen::GenerateOrderStatus(std::vector<LockRequest*>& requests) {
  int tid = tpcc_txn_id;
  int wid = tpcc_machine_id * WPM + URAND(WPM);
  int y = URAND(100);

  if (y < 60) {
    int cid = NURAND(256, 1000) % NUM_CUSTOMER_LOCK;
    GEN_REQUEST(tid, wid, ORDER_STATUS, SHARED, CUSTOMER_START_IDX[wid] + cid);
  } else {
    int cid = NURAND(1024, 3000) % NUM_CUSTOMER_LOCK;
    GEN_REQUEST(tid, wid, ORDER_STATUS, SHARED, CUSTOMER_START_IDX[wid] + cid);
  }

  int olk = URAND(NUM_ORDER_LOCK);
  GEN_REQUEST(tid, wid, ORDER_STATUS, SHARED, ORDER_START_IDX[wid] + olk);

  int ollk = URAND(NUM_ORDER_LINE_LOCK);
  GEN_REQUEST(tid, wid, ORDER_STATUS, SHARED, ORDER_LINE_START_IDX[wid] + ollk);
}

/* Delivery Transaction: TPCC Spec Page 40 of 130
 */
void TPCCLockGen::GenerateDelivery(std::vector<LockRequest*>& requests) {
  int tid = tpcc_txn_id;
  int wid = tpcc_machine_id * WPM + URAND(WPM);

  int nolk = URAND(NUM_NEW_ORDER_LOCK);
  GEN_REQUEST(tid, wid, DELIVERY, EXCLUSIVE, NEW_ORDER_START_IDX[wid] + nolk);

  int olk = URAND(NUM_ORDER_LOCK);
  GEN_REQUEST(tid, wid, DELIVERY, EXCLUSIVE, ORDER_START_IDX[wid] + olk);

  int ollk = URAND(NUM_ORDER_LINE_LOCK);
  GEN_REQUEST(tid, wid, DELIVERY, SHARED, ORDER_LINE_START_IDX[wid] + ollk);

  // for (int i = 0; i < NUM_DISTRICT; i++)
  int cid = NURAND(1024, 3000) % NUM_CUSTOMER_LOCK;
  GEN_REQUEST(tid, wid, DELIVERY, EXCLUSIVE, CUSTOMER_START_IDX[wid] + cid);
}

/* Stock Level Transaction: TPCC Spec Page 44 of 130
 */
void TPCCLockGen::GenerateStockLevel(std::vector<LockRequest*>& requests) {
  int tid = tpcc_txn_id;
  int wid = tpcc_machine_id * WPM + URAND(WPM);

  int did = URAND(NUM_DISTRICT_LOCK);
  GEN_REQUEST(tid, wid, STOCK_LEVEL, SHARED, DISTRICT_START_IDX[wid] + did);

  int ollk = URAND(NUM_ORDER_LINE_LOCK);
  GEN_REQUEST(tid, wid, STOCK_LEVEL, SHARED, ORDER_LINE_START_IDX[wid] + ollk);

  int sid = URAND(NUM_STOCK_LOCK);
  GEN_REQUEST(tid, wid, STOCK_LEVEL, SHARED, STOCK_START_IDX[wid] + sid);
}
