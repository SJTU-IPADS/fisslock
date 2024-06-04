#include <iostream>
#include "tatp_lock_gen.h"

int main(int argc, char* argv[]) {
    int host_num = atoi(argv[1]);
    int txn_count = atoi(argv[2]);
    char* trace_dir = argv[3];
    int txn_start = 1;

    PopulateTables(100000);
    fprintf(stderr, "Total lock num: %d\n", global_lock_id);

    for (int host_id = 1; host_id <= host_num; host_id++) {
        for (int txn_id = txn_start; txn_id < txn_start + txn_count; txn_id++) {
            int random = GenerateUniformRandom(1, 100);
            if (random <= GET_SUBSCRIBER_DATA_RATIO) {
                GetSubscriberData(txn_id);
            } else if (random <= GET_NEW_DESTINATION_RATIO) {
                GetNewDestination(txn_id);
            } else if (random <= GET_ACCESS_DATA_RATIO) {
                GetAccessData(txn_id);
            } else if (random <= UPDATE_SUBSCRIBER_DATA_RATIO) {
                UpdateSubsriberData(txn_id);
            } else if (random <= UPDATE_LOCATION_RATIO) {
                UpdateLocation(txn_id);
            } else if (random <= INSERT_CALL_FORWARDING_RATIO) {
                InsertCallForwarding(txn_id);
            } else if (random <= DELETE_CALL_FORWARDING_RATIO) {
                DeleteCallForwarding(txn_id);
            }
        }

        PREPARE_OUTPUT(host_id);

        for (auto& request : requests) {
            GEN_OUTPUT(request);
        }

        FINISH_OUTPUT();

        txn_start += txn_count;
        ClearRequests();
    }

    return 0;
}