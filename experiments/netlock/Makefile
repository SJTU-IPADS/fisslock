
all: tatp_trace tpcc_trace ycsb_trace dynamic_trace

tatp_trace:
	mkdir -p $(FISSLOCK_PATH)/build/netlock_len_in_switch
	mkdir -p $(FISSLOCK_PATH)/build/netlock_map
	mkdir -p $(FISSLOCK_PATH)/build/netlock_lock_freq
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) tatp 4

tpcc_trace:
	mkdir -p $(FISSLOCK_PATH)/build/netlock_len_in_switch
	mkdir -p $(FISSLOCK_PATH)/build/netlock_map
	mkdir -p $(FISSLOCK_PATH)/build/netlock_lock_freq
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) tpcc 30

ycsb_trace:
	mkdir -p $(FISSLOCK_PATH)/build/netlock_len_in_switch
	mkdir -p $(FISSLOCK_PATH)/build/netlock_map
	mkdir -p $(FISSLOCK_PATH)/build/netlock_lock_freq
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) ycsb-a-uniform 4
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) ycsb-b-uniform 4
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) ycsb-c-uniform 4
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) ycsb-a-zipf 4
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) ycsb-b-zipf 4
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) ycsb-c-zipf 4

dynamic_trace:
	mkdir -p $(FISSLOCK_PATH)/build/netlock_len_in_switch
	mkdir -p $(FISSLOCK_PATH)/build/netlock_map
	mkdir -p $(FISSLOCK_PATH)/build/netlock_lock_freq
	python3 netlock_analysis.py $(FISSLOCK_PATH) $(FISSLOCK_TRACE_PATH) dynamic 4
