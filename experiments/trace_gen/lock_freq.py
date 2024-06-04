import micro_trace_gen

TRACE_SIZE = 1000000
LOCK_NUM = 1000000

if __name__ == "__main__":
    locks = micro_trace_gen.zipf(TRACE_SIZE, LOCK_NUM)

    lock_freq = {}
    for lid in locks:
        if lid not in lock_freq:
            lock_freq[lid] = 0
        lock_freq[lid] += 1

    for lid, freq in lock_freq.items():
        print("Lock: {} freq: {}".format(lid, freq))