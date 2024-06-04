
import sys
from datetime import datetime
from numpy.random import default_rng
import threading

THREADS = []

############ Configurations.
TRACE_SIZE = 1000000

DYNAMIC_HOT_REQUEST_PERCENT = 50
DYNAMIC_HOT_LOCK_NUM = 2500

############ The generator for simulating the dynamic workload.
class skew():
    def __init__(self, lock_num, hot_num, skew_percent):
        self.lock_num = lock_num
        self.hot_num = hot_num
        self.skew_percent = skew_percent
    
    def get_one(self):
        u = int(rng.uniform(1, 100))
        return int(rng.uniform(0, (
            self.hot_num 
            if u <= self.skew_percent 
            else self.lock_num
        ) - 1))

############ The zipf generator used by YCSB.
class zipf_ycsb():
    zipfconstant = 0.99

    def zeta(self, upper, theta, low=0):
        sum = 0.0
        for i in range(low, upper):
            sum += 1.0 / pow(i + 1, theta)
        return sum

    def __init__(self, range_l, range_r, factor):
        self.items = range_r - range_l + 1
        self.base = range_l
        self.zipfconstant = factor

        theta = self.zipfconstant
        self.alpha = 1.0 / (1.0 - theta)
        self.zeta_2 = self.zeta(2, theta)
        self.zeta_n = self.zeta(self.items, theta)
        self.eta = (1 - pow(2.0 / self.items, 1 - theta)) / \
                   (1 - self.zeta_2 / self.zeta_n)

    def get_one(self):
        u = rng.uniform()
        uz = u * self.zeta_n

        if uz < 1.0:
            return self.base
        elif uz < 1.0 + pow(0.5, self.zipfconstant):
            return self.base + 1
        else:
            return self.base + int(self.items * \
                pow(self.eta * u - self.eta + 1, self.alpha))

############ Constants and maps.
MILLION = 1000000
LOCK_SHARED = 1
LOCK_EXCLUSIVE = 2

rng = default_rng()
zipf_generator = { MILLION: zipf_ycsb(0, MILLION - 1, 0.90) }

############ The generic trace generating function.
def generic_generator(out_file, distributer, write_ratio, lock_num, partition):
    trace_list = []

    # Generate a scalar of accessed locks and access modes 
    # following the specified distribution and write ratio.
    locks = distributer(TRACE_SIZE, lock_num)
    ops = [LOCK_EXCLUSIVE if op < write_ratio else LOCK_SHARED 
        for op in rng.integers(0, 100, TRACE_SIZE)]

    # Generate a line of trace for each access.
    tid_offset = partition * TRACE_SIZE
    for i in range(len(locks)):
        trace_list.append("{},{},1,{},{}\n".format(
            tid_offset + i + 1, 0, locks[i], ops[i]
        ))
    
    # Write to the output file.
    file = open("{}/{}.csv".format(TRACE_DIR, out_file), 'w')
    file.writelines(trace_list)
    file.close()

def generate(out_file, distributer, write_ratio, lock_num, partition):
    t = threading.Thread(
        target=generic_generator, 
        args = (out_file, distributer, write_ratio, lock_num, partition)
    )
    t.start()
    THREADS.append(t)

############ Distributers.
def uniform(trace_len, lock_num):
    return [int(lock * lock_num) for lock in rng.uniform(size=trace_len)]

def zipf(trace_len, lock_num):
    zipfg = zipf_generator[lock_num]
    return [zipfg.get_one() for _ in range(trace_len)]

def same(trace_len, lock_num):
    return [1 for _ in range(trace_len)]

def dynamic(trace_len, lock_num):
    skewg = skew(MILLION, DYNAMIC_HOT_LOCK_NUM, DYNAMIC_HOT_REQUEST_PERCENT)
    return [skewg.get_one() for _ in range(trace_len)] 

############ Main function.

if __name__ == "__main__":

    if len(sys.argv) < 3:
        print("usage: python3 micro_trace_gen.py <out_dir> <host_num> <test>")
        sys.exit(0)

    TRACE_DIR = sys.argv[1]
    HOST_NUM = int(sys.argv[2])
    TEST = sys.argv[3]

    for i in range(HOST_NUM):
        host = i + 1

        # Microbenchmark 
        if TEST == "micro":
            generate("h{}-micro-uh-uni".format(host), uniform, 50, MILLION, i)
            generate("h{}-micro-rm-uni".format(host), uniform, 10, MILLION, i)
            generate("h{}-micro-ro-uni".format(host), uniform, 0, MILLION, i)
            generate("h{}-micro-uh-zipf".format(host), zipf, 50, MILLION, i)
            generate("h{}-micro-rm-zipf".format(host), zipf, 10, MILLION, i)
            generate("h{}-micro-ro-zipf".format(host), zipf, 0, MILLION, i)

        # Dynamic test
        if TEST == "dynamic":
            generate("h{}-dynamic".format(host), dynamic, 0, MILLION, i)

        # Lock scale test
        if TEST == "lock-scale":
            generate("h{}-lkscale-1m".format(host), uniform, 10, MILLION, i)
            generate("h{}-lkscale-2m".format(host), uniform, 10, 2*MILLION, i)
            generate("h{}-lkscale-5m".format(host), uniform, 10, 5*MILLION, i)
            generate("h{}-lkscale-10m".format(host), uniform, 10, 10*MILLION, i)

    for t in THREADS:
        t.join()