import os, sys
import re

FISSLOCK_PATH = sys.argv[1]
TRACES_PATH = sys.argv[2]
BENCHMARK = sys.argv[3]
CRT_NUM = int(sys.argv[4])

MAP_PATH = FISSLOCK_PATH + "/build/netlock_map"
LEN_IN_SWITCH_PATH = FISSLOCK_PATH + "/build/netlock_len_in_switch"
LOCK_FREQ_PATH = FISSLOCK_PATH + "/build/netlock_lock_freq"

tx_core_num_map = {
    "1": 4,
    "2": 6,
    "3": 6,
    "4": 6,
    "5": 6,
    "6": 6,
    # "7": 6,
    "8": 6,
}

# Configuration
MAX_SLOT_NUM = 100000
DYNAMIC_TRANS_PHASE = 1
GEN_OUTPUT_FILE = True

# Global
benchmark_files = {}

def collect_files():
    trace_file_list = os.listdir(TRACES_PATH)
    benchmark_files[BENCHMARK] = []
    for file in trace_file_list:
        if BENCHMARK in file:
            if not ("h1-" in file or
                    "h2-" in file or
                    "h3-" in file or
                    "h4-" in file or
                    "h5-" in file or
                    "h6-" in file or
                    # "h7-" in file or
                    "h8-" in file):
                continue
            benchmark_files[BENCHMARK].append(file)
                
def analysis(benchmark):
    obj_count = {}
    obj_in_file = {}
    first_phase_obj_count = {}
    first_phase_obj_in_file = {}
    read_count = 0
    write_count = 0

    if len(benchmark_files) == 0:
        print("Cannot find {} benchmark files".format(benchmark))
        return

    for file in benchmark_files[benchmark]:
        match_result = re.match(r"^h(\d+)-.*", file)
        if not match_result:
            print("Cannot get host id of benchmark file {}".format(file));
            exit(1)
        host_id = match_result.group(1)
        print("analyze {}".format(file))
        fin = open(TRACES_PATH + "/" + file, "r")
        lines = fin.readlines()
        trace_id = 0
        for line in lines:
            words = line.split(',')
            action_type = int(words[1])
            obj_idx = int(words[3])
            lock_type = int(words[4])
            if lock_type == 1:
                read_count += 1
            elif lock_type == 2:
                write_count += 1
            if action_type != 0:
                continue
            if obj_idx not in obj_count:
                obj_count[obj_idx] = 0
                obj_in_file[obj_idx] = {}
            obj_count[obj_idx] += 1
            tx_core_id = trace_id % (tx_core_num_map[host_id] * CRT_NUM)
            trace_id += 1
            obj_in_file[obj_idx][file + "-t{}".format(tx_core_id)] = 1
        fin.close()
    print("Read count: {}, Write count: {}".format(read_count, write_count))
    obj_value = {}

    # Lock frequency
    if GEN_OUTPUT_FILE == True:
        lock_freq_file = open(LOCK_FREQ_PATH + "/" + benchmark, "w+")
        sorted_obj_count = sorted(obj_count.items(), key = lambda x:x[1], reverse=True)
        for obj in sorted_obj_count:
            lock_freq_file.write("{},{}\n".format(obj[0], obj[1]))
        lock_freq_file.close()

    # Allocate on-switch lock
    for obj_id in obj_count:
        current_value = obj_count[obj_id] / len(obj_in_file[obj_id])
        obj_value[obj_id] = current_value
    sorted_obj = sorted(obj_value.items(), key = lambda x:x[1], reverse=True)
    if GEN_OUTPUT_FILE == True:
        map_file = open(MAP_PATH + "/" + benchmark, "w+")
        len_in_switch_file = open(LEN_IN_SWITCH_PATH + "/" + benchmark, "w+")
        current_occupied_slots = 0
        current_lock_id = 0
        for obj in sorted_obj:
            obj_id = obj[0]
            map_file.write("{},{}\n".format(obj_id, current_lock_id))
            if current_occupied_slots < MAX_SLOT_NUM:
                current_queue_size = min(MAX_SLOT_NUM - current_occupied_slots, len(obj_in_file[obj_id]))
                len_in_switch_file.write("{},{}\n".format(current_lock_id, current_queue_size))
                current_occupied_slots += current_queue_size
            current_lock_id += 1
        map_file.close()
        len_in_switch_file.close()

    sorted_first_phase_obj_count = sorted(first_phase_obj_count.items(), key = lambda x:x[0])
    agg = 0
    prev_phase = 0
    for key, value in sorted_first_phase_obj_count:
        cur_phase = int((agg + value) / (100 * 10000))
        if cur_phase > prev_phase:
            print("{}, {}".format(key, agg + value))
            prev_phase = cur_phase
        agg += value
        
collect_files()
print("analyze benchmark {}".format(BENCHMARK))
analysis(BENCHMARK)
