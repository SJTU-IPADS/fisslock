#include <random>
#include "tatp_lock_gen.h"

// #define UNIFORM_DISTRIBUTION

SubscriberTable subscriber;
AccessInfoTable access_info;
SpecialFacilityTable special_facility;
CallForwardingTable call_forwarding;

std::random_device rd;
std::mt19937 gen(rd());

static int subscriber_size = 0;
int global_lock_id = ENTRY_LOCK_START;
static int cf_lock_begin = 0;
static int cf_lock_num = 0;
vector<LockRequest> requests;
FILE* of = NULL;

void PopulateSubscriber(int size);
void PopulateAccessInfo();
void PopulateSpecialFacility();
void PopulateCallForwarding();

static inline int GetCallForwardingLock(int s_id, int sf_type, int start_time) {
    return (Hash(s_id, sf_type, start_time) % cf_lock_num) + cf_lock_begin;
}

int GenerateUniformRandom(int start, int end) {
    std::uniform_int_distribution<> dis(start, end);
    return dis(gen);
}

int GenerateRandom(int start, int end) {
#ifdef UNIFORM_DISTRIBUTION
    return GenerateUniformRandom(start, end);
#else
    int A = 65535;
    return ((GenerateUniformRandom(0, A) | GenerateUniformRandom(start, end)) 
        % (end - start + 1)) + start;
#endif
}

void ClearRequests() {
    requests.clear();
}

void PopulateTables(int size) {
    subscriber_size = size;
    PopulateSubscriber(size);
    PopulateAccessInfo();
    PopulateSpecialFacility();
    cf_lock_begin = global_lock_id;
    cf_lock_num = subscriber_size * 4 / CALL_FORWARDING_LOCK_FACTOR;
    global_lock_id += cf_lock_num;
    PopulateCallForwarding();
}

void PopulateSubscriber(int size) {
    for (int s_id = 1; s_id <= size; s_id++) {
        subscriber.emplace(s_id, SubscriberEntry{s_id, global_lock_id++});
    }
}

void PopulateAccessInfo() {
    for (auto& iter : subscriber) {
        int cardinality = GenerateUniformRandom(1,4);
        int start = GenerateUniformRandom(1,4);
        unordered_map<int, AccessInfoEntry> current_map;
        for (int i = 0; i < cardinality; i++) {
            current_map.emplace(
                start, 
                AccessInfoEntry{iter.first, start, global_lock_id++}
            );
            start = start % 4 + 1;
        }
        access_info.insert({iter.first, current_map});
    }
}

void PopulateSpecialFacility() {
    for (auto& iter : subscriber) {
        int cardinality = GenerateUniformRandom(1, 4);
        int start = GenerateUniformRandom(1, 4);
        unordered_map<int, SpecialFacilityEntry> current_map;
        for (int i = 0; i < cardinality; i++) {
            int is_active = (GenerateUniformRandom(1, 100) <= 85) ? 1 : 0;
            current_map.emplace(
                start,
                SpecialFacilityEntry{iter.first, start, is_active, global_lock_id++}
            );
            start = start % 4 + 1;
        }
        special_facility.insert({iter.first, current_map});
    }
}

void PopulateCallForwarding() {
    for (auto& iter : special_facility) {
        int s_id = iter.first;
        for (auto& entry : iter.second) {
            unordered_map<int, CallForwardingEntry> current_map;
            int sf_type = entry.second.sf_type;
            int cardinality = GenerateUniformRandom(0, 3);
            if (!cardinality) continue;
            int start = GenerateUniformRandom(0, 2);
            for (int i = 0; i < cardinality; i++) {
                int duration = GenerateUniformRandom(1, 8);
                current_map.emplace(
                    start * 8,
                    CallForwardingEntry{s_id, sf_type, start * 8, 
                        start * 8 + duration, GetCallForwardingLock(s_id, sf_type, start * 8)}
                );
                start = (start + 1) % 2;
            }
            call_forwarding.insert({
                static_cast<int64_t>(s_id) << 32 | sf_type,
                current_map
            });
        }
    }
}

/*
SELECT s_id, sub_nbr, 
 bit_1, bit_2, bit_3, bit_4, bit_5, bit_6, bit_7, 
 bit_8, bit_9, bit_10, 
 hex_1, hex_2, hex_3, hex_4, hex_5, hex_6, hex_7, 
 hex_8, hex_9, hex_10, 
 byte2_1, byte2_2, byte2_3, byte2_4, byte2_5, 
 byte2_6, byte2_7, byte2_8, byte2_9, byte2_10, 
 msc_location, vlr_location 
FROM Subscriber 
WHERE s_id = <s_id rnd>; 
*/
void GetSubscriberData(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int lock_id = subscriber.find(s_id)->second.lock_id;
    GEN_REQUEST(txn_id, lock_id, SHARED, GET_SUBSCRIBER_DATA_TYPE);
}

/*
SELECT cf.numberx 
FROM Special_Facility AS sf, Call_Forwarding AS cf 
WHERE 
 (sf.s_id = <s_id rnd> 
 AND sf.sf_type = <sf_type rnd> 
 AND sf.is_active = 1) 
 AND (cf.s_id = sf.s_id 
 AND cf.sf_type = sf.sf_type) 
 AND (cf.start_time \<= <start_time rnd> 
 AND <end_time rnd> \< cf.end_time); 
*/
void GetNewDestination(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int sf_type = GenerateRandom(1, 4);
    auto pos = special_facility[s_id].find(sf_type);
    if (pos != special_facility[s_id].end()) {
        GEN_REQUEST(txn_id, pos->second.lock_id, 
                SHARED, GET_NEW_DESTINATION_TYPE);
    }
    auto cf_pos = call_forwarding.find(static_cast<int64_t>(s_id) << 32 | sf_type);
    if (cf_pos == call_forwarding.end()) return;
    int start_time = GenerateRandom(0, 2) * 8;
    int end_time = GenerateRandom(1, 24);
    unordered_map<int,bool> used_lock;
    for (int i = 0; i <= start_time; i += 8) {
        if (cf_pos->second.find(i) != cf_pos->second.end() && cf_pos->second.find(i)->second.end_time <= end_time) {
            int lock_id = GetCallForwardingLock(s_id, sf_type, i);
            if (used_lock.count(lock_id)) continue;
            GEN_REQUEST(txn_id, GetCallForwardingLock(s_id, sf_type, i), 
                        SHARED, GET_NEW_DESTINATION_TYPE); 
            used_lock.insert({lock_id, true});
        }
    }
}

/*
SELECT data1, data2, data3, data4 
FROM Access_Info 
WHERE s_id = <s_id rnd> 
 AND ai_type = <ai_type rnd> 
*/
void GetAccessData(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int ai_type = GenerateRandom(1, 4);
    auto pos = access_info[s_id].find(ai_type);
    if (pos == access_info[s_id].end()) return;
    int lock_id = pos->second.lock_id;
    GEN_REQUEST(txn_id, lock_id, SHARED, GET_ACCESS_DATA_TYPE);
}

/*
UPDATE Subscriber 
SET bit_1 = <bit_rnd> 
WHERE s_id = <s_id rnd subid>; 

UPDATE Special_Facility 
SET data_a = <data_a rnd> 
WHERE s_id = <s_id value subid> 
 AND sf_type = <sf_type rnd>;
*/
void UpdateSubsriberData(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int sf_type = GenerateRandom(1, 4);
    int subs_lock = subscriber.find(s_id)->second.lock_id;
    GEN_REQUEST(txn_id, subs_lock, 
        EXCLUSIVE, UPDATE_SUBSCRIBER_DATA_TYPE);
    auto pos = special_facility[s_id].find(sf_type);
    if (pos == special_facility[s_id].end()) return;
    int sf_lock = pos->second.lock_id;
    GEN_REQUEST(txn_id, sf_lock, 
        EXCLUSIVE, UPDATE_SUBSCRIBER_DATA_TYPE);
}

/*
UPDATE Subscriber 
SET vlr_location = <vlr_location rnd> 
WHERE sub_nbr = <sub_nbr rndstr>; 
*/
void UpdateLocation(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int lock_id = subscriber.find(s_id)->second.lock_id;
    GEN_REQUEST(txn_id, lock_id, EXCLUSIVE, UPDATE_LOCATION_TYPE);
}

/*
SELECT <s_id bind subid s_id> 
FROM Subscriber 
WHERE sub_nbr = <sub_nbr rndstr>; 

SELECT <sf_type bind sfid sf_type> 
FROM Special_Facility 
WHERE s_id = <s_id value subid>: 

INSERT INTO Call_Forwarding 
VALUES (<s_id value subid>, <sf_type rnd sf_type>, 
 <start_time rnd>, <end_time rnd>, <numberx rndstr>);
*/
void InsertCallForwarding(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int subs_lock = subscriber.find(s_id)->second.lock_id;
    GEN_REQUEST(txn_id, subs_lock, SHARED, INSERT_CALL_FORWARDING_TYPE);

    for (auto& sf_entry : special_facility[s_id]) {
        int sf_lock = sf_entry.second.lock_id;
        GEN_REQUEST(txn_id, sf_lock, 
            SHARED, INSERT_CALL_FORWARDING_TYPE);
    }
    int sf_type = GenerateRandom(1, 4);
    int start_time = GenerateRandom(0, 2) * 8;
    int end_time = GenerateRandom(1, 24);

    // Foreign key constraint
    if (special_facility[s_id].find(sf_type) == special_facility[s_id].end()) return;

    // Primary key constraint
    if (call_forwarding[static_cast<int64_t>(s_id) << 32 | sf_type].count(start_time) == 0) return;
    GEN_REQUEST(txn_id, GetCallForwardingLock(s_id, sf_type, start_time), 
        EXCLUSIVE, INSERT_CALL_FORWARDING_TYPE);
    call_forwarding.insert({
        static_cast<int64_t>(s_id) << 32 | sf_type,
        unordered_map<int, CallForwardingEntry> {{
            start_time, CallForwardingEntry{
               s_id, sf_type, start_time, end_time,
               GetCallForwardingLock(s_id, sf_type, start_time) 
            }
        }}
    });
}

/*
SELECT <s_id bind subid s_id> 
FROM Subscriber 
WHERE sub_nbr = <sub_nbr rndstr>; 

DELETE FROM Call_Forwarding 
WHERE s_id = <s_id value subid>
AND sf_type = <sf_type rnd> 
 AND start_time = <start_time rnd>; 
*/
void DeleteCallForwarding(int txn_id) {
    int s_id = GenerateRandom(1, subscriber_size);
    int subs_lock = subscriber.find(s_id)->second.lock_id;
    GEN_REQUEST(txn_id, subs_lock, SHARED, DELETE_CALL_FORWARDING_TYPE);

    int sf_type = GenerateRandom(1, 4);
    int start_time = GenerateRandom(0, 2) * 8;
    if (call_forwarding[static_cast<int64_t>(s_id) << 32 | sf_type].count(start_time) == 0) return;
    GEN_REQUEST(txn_id, GetCallForwardingLock(s_id, sf_type, start_time), 
        EXCLUSIVE, DELETE_CALL_FORWARDING_TYPE);
    call_forwarding[static_cast<int64_t>(s_id) << 32 | sf_type].erase(start_time);
}