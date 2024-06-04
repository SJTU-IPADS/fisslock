
SHELL := bash
TPCC_TXN_NUM := 100000

all: main

main: main.cc tpcc_lock_gen.cc tpcc_lock_gen.h
	$(CXX) main.cc tpcc_lock_gen.cc -o main

traces: main
	@host=1; txn_start=1; while [[ $$host -le $$HOST_NUM ]] ; do \
		echo "Generating the TPCC trace for host $$host..."; \
		./main $$txn_start $(TPCC_TXN_NUM) $$host \
			>$$FISSLOCK_TRACE_PATH/h$$host-tpcc.csv; \
		((host = host + 1)); \
		((txn_start = txn_start + $(TPCC_TXN_NUM))); \
	done

clean:
	rm -rf main