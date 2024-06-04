#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <utility>
#include <assert.h>
#include <thread>
#include <vector>
#include <set>

using namespace std;
using namespace chrono;

int switch_direct_grant_counter = 0;
int server_direct_grant_counter = 0;
int switch_competitive_counter = 0;
int server_competitive_counter = 0;
std::vector<uint32_t> switch_direct_grant_lock;
std::set<uint32_t> switch_direct_grant_lock_set;
#define GET_LOCK_ID(lktsk) ((uint32_t)(lktsk >> 32))

enum timer_type {
	TIMER_ACQ = 1,
	TIMER_REL,
	TIMER_REL_LOCAL,
	TIMER_SCHED,
	TIMER_GRANT_LOCAL,
	TIMER_GRANT_W_AGENT,
	TIMER_GRANT_WO_AGENT,
	TIMER_HANDLE_ACQUIRE,
};

#define RTT 4000
#define THREAD_NUM 1 

unordered_map<uint64_t, timer_type> t_type;
unordered_map<uint64_t, uint64_t> atime;
unordered_map<uint64_t, uint64_t> htime;
unordered_map<uint64_t, uint64_t> gwtime;
unordered_map<uint64_t, uint64_t> gntime;
unordered_map<uint64_t, uint64_t> gstime;
unordered_map<uint64_t, uint64_t> gltime;
unordered_map<uint64_t, uint64_t> rtime;
unordered_map<uint64_t, uint64_t> rltime;
unordered_map<uint64_t, uint64_t> sctime;
unordered_map<uint64_t, uint64_t> btime;
unordered_map<uint64_t, uint64_t> getime;

unordered_map<uint64_t, uint64_t> scheduled_from;

unordered_map<uint32_t, vector<double>> txn_e2etime;

char *system_name;

std::ifstream input_raw_fs;

#define OUT_FILE(name) \
std::ofstream output_##name##_fs; \
std::vector<double> output_##name##_array[THREAD_NUM]

#define OUT_FILE_PREPARE(name) \
	auto output_filename_##name = string(test) + "-" + #name;\
	output_##name##_fs.open(output_filename_##name, fstream::trunc);\
	assert(output_##name##_fs.is_open())

#define IN_FILE_PREPARE(name) \
	auto input_filename_##name = string(test) + "-" + #name;\
	input_##name##_fs.open(input_filename_##name, fstream::in);\
	assert(input_##name##_fs.is_open())

#define LATENCY_OUT(latency_ns, name, tid) \
	output_##name##_array[tid].push_back(latency_ns / 1000.0)

// Breakdown latencies.
OUT_FILE(grant); /* grantable grant latency */
OUT_FILE(wait); /* wait time */

void handle_an_acquire(uint64_t lktsk, int tid) {
	uint64_t gtime = 0;
	assert(getime[lktsk]);

	if (!strcmp(system_name, "fisslock") || !strcmp(system_name, "parlock")) {

		if (sctime[lktsk]) { /* case 3/5 */
			uint64_t from = scheduled_from[lktsk];
			assert(from);
			assert(rtime[lktsk] || rltime[lktsk]);

			gtime = gwtime[lktsk] ? gwtime[lktsk] :
						 (gntime[lktsk] ? gntime[lktsk] : gltime[lktsk]);

			gtime += (rtime[from]) ? (rtime[from] + RTT + sctime[lktsk]) 
														 : (rltime[from] + 0.5 * RTT + sctime[lktsk]);
			LATENCY_OUT(gtime, grant, tid);
			LATENCY_OUT(getime[lktsk] - gtime, wait, tid);

		} else { /* case 1/2/4 */
			LATENCY_OUT(getime[lktsk], grant, tid);
			LATENCY_OUT(0, wait, tid);
		}

	} else if (!strcmp(system_name, "netlock")) {

		// The request is handled by the primary server and is not
		// immediately granted.
		if (sctime[lktsk]) {
			server_competitive_counter += 1;
			uint64_t from = scheduled_from[lktsk];
			assert(gntime[lktsk] && from && rtime[from]);

			gtime = rtime[from] + RTT + sctime[lktsk] + gntime[lktsk];
			LATENCY_OUT(gtime, grant, tid);
			LATENCY_OUT(getime[lktsk] - gtime, wait, tid);

		// The request is handled by the primary server but is 
		// immediately granted.
		} else if (htime[lktsk]) {
			server_direct_grant_counter += 1;
			LATENCY_OUT(getime[lktsk], grant, tid);
			LATENCY_OUT(0, wait, tid);

		// The request is handled by the switch.
		} else {
			if (gstime[lktsk]) { /* Grantable */
				switch_direct_grant_counter += 1;
				switch_direct_grant_lock.push_back(GET_LOCK_ID(lktsk));
				switch_direct_grant_lock_set.insert(GET_LOCK_ID(lktsk));
				LATENCY_OUT(getime[lktsk], grant, tid);
				LATENCY_OUT(0, wait, tid);

			} else { /* Competitive */
				switch_competitive_counter += 1;

				// scheduled_from not trackable here, we simulate the
				// latency with atime instead.
				gtime = atime[lktsk] + 0.5 * RTT + gntime[lktsk] + btime[lktsk];
				LATENCY_OUT(gtime, grant, tid);
				LATENCY_OUT(getime[lktsk] - gtime, wait, tid);
			}
		}

	} else if (!strcmp(system_name, "srvlock")) {

		if (sctime[lktsk]) { /* Competitive */
			uint64_t from = scheduled_from[lktsk];
			assert(from && rtime[from]);
			gtime = rtime[from] + RTT + sctime[lktsk] + gntime[lktsk];
			LATENCY_OUT(gtime, grant, tid);
			LATENCY_OUT(getime[lktsk] - gtime, wait, tid);

		} else { /* Grantable */
			LATENCY_OUT(getime[lktsk], grant, tid);
			LATENCY_OUT(0, wait, tid);
		} 
	}
}

void worker(int tid) {
	for (auto const &i : atime) {
		if (i.first % THREAD_NUM == (uint64_t)tid) {
			handle_an_acquire(i.first, tid);
		}
	}
	printf("switch_direct_grant: %d, server_direct_grant: %d\n", switch_direct_grant_counter, server_direct_grant_counter);
	printf("switch competitive count: %d, server competitive count: %d\n", switch_competitive_counter, server_competitive_counter);
	printf("switch direct grant lock num is %lu\n", switch_direct_grant_lock_set.size());
}

int main() {
	char* test = getenv("TEST_NAME");
	system_name = getenv("SYSTEM_NAME");
	assert(system_name);

	IN_FILE_PREPARE(raw);
	OUT_FILE_PREPARE(grant); 
	OUT_FILE_PREPARE(wait);

	printf("Loading log files from disk...\n");

	uint64_t from, lktsk;
	char type[5] = {'\0'};
	int64_t lat;
	string line;
	while (getline(input_raw_fs, line)) {
		sscanf(line.c_str(), "%lx,%lx,%ld,%[a-z]\n", 
			&lktsk, &from, &lat, type);
		
		if (type[0] == 'r') { 
			if (type[1] == 'l') rltime[lktsk] = lat; 
			else rtime[lktsk] = lat; 
		} else if (type[0] == 's') { 
			sctime[lktsk] = lat; 
			scheduled_from[lktsk] = from; 
		} else if (type[0] == 'b') btime[lktsk] = lat; 
		else if (type[0] == 'a') atime[lktsk] = lat;
		else if (type[0] == 'h') htime[lktsk] = lat;
		else if (type[0] == 'g') {
			switch (type[1]) {
				case 'w': gwtime[lktsk] = lat; break;
				case 'n': gntime[lktsk] = lat; break;
				case 'l': gltime[lktsk] = lat; break;
				case 'e': getime[lktsk] = lat; break;
				case 's': gstime[lktsk] = lat; break;
				default: assert(0);
			}
		} else {
			// We do not recognize other types of logs.
		}
	}

	printf("Spawning worker threads...\n");

	thread thread_array[THREAD_NUM];
	for (int tid = 0; tid < THREAD_NUM; tid++) {
		thread_array[tid] = thread(worker, tid);
	}
	for (int tid = 0; tid < THREAD_NUM; tid++) {
		thread_array[tid].join();
	}

	printf("Writing output to file...\n");

	for (int tid = 0; tid < THREAD_NUM; tid++) {
		for (auto iter : output_grant_array[tid]) {
			output_grant_fs << std::fixed << iter << endl;
		}

		for (auto iter : output_wait_array[tid]) {
			output_wait_fs << std::fixed << iter << endl;
		}
	}

	return 0;
}
