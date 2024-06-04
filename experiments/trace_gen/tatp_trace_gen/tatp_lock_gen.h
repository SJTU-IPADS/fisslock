#include <unordered_map>
#include <vector>
#include <set>
#include <stdint.h>
#include <cstdio>
#include <stdio.h>

using std::unordered_map;
using std::vector;
using std::set;

struct SubscriberEntry {
    int s_id;
    int lock_id;
    SubscriberEntry(int s_id_, int lock_id_) : s_id(s_id_), lock_id(lock_id_) {}
};

struct AccessInfoEntry {
    int s_id;
    int ai_type;
    int lock_id;
    AccessInfoEntry(int s_id_, int ai_type_, int lock_id_) 
        : s_id(s_id_), ai_type(ai_type_), lock_id(lock_id_) {}
};

struct SpecialFacilityEntry {
    int s_id;
    int sf_type;
    int is_active;
    int lock_id;
    SpecialFacilityEntry(int s_id_, int sf_type_, int is_active_, int lock_id_)
        : s_id(s_id_), sf_type(sf_type_), is_active(is_active_), lock_id(lock_id_) {}
};

struct CallForwardingEntry {
    int s_id;
    int sf_type;
    int start_time;
    int end_time;
    int lock_id;
    CallForwardingEntry(int s_id_, int sf_type_, int start_time_, int end_time_, int lock_id_) :
        s_id(s_id_), sf_type(sf_type_), 
        start_time(start_time_), end_time(end_time_), lock_id(lock_id_) {}
};

using SubscriberTable = unordered_map<int, SubscriberEntry>;
using AccessInfoTable = unordered_map<int, unordered_map<int, AccessInfoEntry>>;
using SpecialFacilityTable = unordered_map<int, unordered_map<int, SpecialFacilityEntry>>;
using CallForwardingTable = unordered_map<int64_t, unordered_map<int, CallForwardingEntry>>;

const int GET_SUBSCRIBER_DATA_RATIO = 35;
const int GET_NEW_DESTINATION_RATIO = 45;
const int GET_ACCESS_DATA_RATIO = 80;
const int UPDATE_SUBSCRIBER_DATA_RATIO = 82;
const int UPDATE_LOCATION_RATIO = 96;
const int INSERT_CALL_FORWARDING_RATIO = 98;
const int DELETE_CALL_FORWARDING_RATIO = 100;

const int GET_SUBSCRIBER_DATA_TYPE = 1;
const int GET_NEW_DESTINATION_TYPE = 2;
const int GET_ACCESS_DATA_TYPE = 3;
const int UPDATE_SUBSCRIBER_DATA_TYPE = 4;
const int UPDATE_LOCATION_TYPE = 5;
const int INSERT_CALL_FORWARDING_TYPE = 6;
const int DELETE_CALL_FORWARDING_TYPE = 7;

const int SUBSCRIBER_TABLE_LOCK = 1;
const int ACCESS_INFO_TABLE_LOCK = 2;
const int SPECIAL_FACILITY_TABLE_LOCK = 3;
const int CALL_FORWARDING_TABLE_LOCK = 4;
const int ENTRY_LOCK_START = 5;
const int CALL_FORWARDING_LOCK_FACTOR = 5;

const int SHARED = 1;
const int EXCLUSIVE = 2;
const int LOCK = 0;
const int UNLOCK = 1;

inline int Hash(int x, int y, int z) {
     // Use a large prime number as a multiplier
    const int P = 1000000007;
    // Use bitwise operations to mix the bits of the inputs
    return ((x ^ y) * P + z) ^ (x + y + z);
}

void PopulateTables(int size);

void GetSubscriberData(int txn_id);
void GetNewDestination(int txn_id);
void GetAccessData(int txn_id);

void UpdateSubsriberData(int txn_id);
void UpdateLocation(int txn_id);
void InsertCallForwarding(int txn_id);
void DeleteCallForwarding(int txn_id);

int GenerateUniformRandom(int start, int end);
int GenerateSkewRandom(int start, int end);

#define PREPARE_OUTPUT(host_id) do {\
    auto filename = std::string(trace_dir) + "/h" + \
        std::to_string(host_id) + "-tatp.csv";\
    of = fopen(filename.c_str(), "w");\
} while (0)

#define FINISH_OUTPUT() do {\
    fclose(of);\
} while (0)

#define GEN_OUTPUT(lr) do {\
  fprintf(of, "%d,%d,%d,%d,%d\n", lr.txn_id, lr.task, lr.txn_type,\
    lr.lock_id, lr.lock_type);\
} while (0)

#define GEN_REQUEST(txn_id, lock_id, lock_type, txn_type) do {\
    requests.emplace_back(LockRequest{txn_id, LOCK, lock_id, lock_type, txn_type});\
} while (0)

void ClearRequests();


struct LockRequest {
  int txn_id;
  int task;      // lock, unlock
  int lock_id; // lock id
  int lock_type; // shared, exclusive
  int txn_type;
};

extern vector<LockRequest> requests;
extern int global_lock_id;
extern FILE* of;

