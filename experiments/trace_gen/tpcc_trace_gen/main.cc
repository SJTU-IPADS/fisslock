#include <vector>
#include "tpcc_lock_gen.h"

int main(int argc, char* argv[]) {
    int txn_start = atoi(argv[1]);
    int txn_count = atoi(argv[2]);
    int machine_id = atoi(argv[3]);
    TPCCLockGen generator(machine_id - 1);
    for (int i = txn_start; i < txn_start + txn_count; i++) {
        std::vector<LockRequest*> requests;
        generator.Generate(i, requests);
        for (auto request : requests) {
            GEN_OUTPUT(request);
        }
    }
}