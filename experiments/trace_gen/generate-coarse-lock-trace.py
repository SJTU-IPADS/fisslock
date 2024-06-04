import os, sys
import re

FISSLOCK_PATH = "/home/ck/workspace/pslock"
TRACES_PATH = FISSLOCK_PATH + "/traces"

target_benchmark = ['ycsb-b-uniform']

benchmark_files = {}

LOCK_NUM = 750
LOCK_FACTOR = int(1000000 / LOCK_NUM)

lock_map = {}

def collect_files():
    trace_file_list = os.listdir(TRACES_PATH)
    for file in trace_file_list:
        for benchmark in target_benchmark:
            if benchmark not in benchmark_files:
                benchmark_files[benchmark] = []
            if benchmark in file:
                if not ("h1-" in file or
                        "h2-" in file or
                        "h3-" in file or
                        "h4-" in file or
                        "h5-" in file or
                        "h6-" in file or
                        "h7-" in file or
                        "h8-" in file) or "coarse" in file or "no_local" in file:
                    continue
                benchmark_files[benchmark].append(file)

def generate_new_trace(benchmark):
    for file in benchmark_files[benchmark]:
        print("Generate new trace for file {}".format(file))
        original_filename = file.rsplit('.', 1)[0]
        new_filename = original_filename + '-coarse2-' + str(LOCK_NUM)
        fin = open(TRACES_PATH + "/" + file, "r")
        fout = open(TRACES_PATH + "/" + new_filename + ".csv", "w")
        print("New trace file {}".format(TRACES_PATH + "/" + new_filename + ".csv"))
        lines = fin.readlines()
        for line in lines:
            a, b, c, d, e = line.strip().split(',')
            # d_coarse = str(int(d) % LOCK_NUM)
            d_coarse = str(int(int(d) / LOCK_FACTOR) * LOCK_FACTOR)
            if d_coarse not in lock_map:
                lock_map[d_coarse] = 1

            new_line = f"{a},{b},{c},{d_coarse},{e}\n"
            fout.write(new_line)

    print(len(lock_map))

if __name__ == "__main__":
    collect_files()
    for benchmark in target_benchmark:
        generate_new_trace(benchmark)