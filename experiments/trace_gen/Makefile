
HOST_NUM := 8

all: tatp_trace tpcc_trace ycsb_trace dynamic_trace lkscale_trace

tatp_trace: trace_dir
	make -C tatp_trace_gen traces HOST_NUM=$(HOST_NUM)
	
tpcc_trace: trace_dir
	make -C tpcc_trace_gen traces HOST_NUM=$(HOST_NUM)

micro_trace: trace_dir
	python3 micro_trace_gen.py $(FISSLOCK_TRACE_PATH) $(HOST_NUM) micro

dynamic_trace: trace_dir
	python3 micro_trace_gen.py $(FISSLOCK_TRACE_PATH) $(HOST_NUM) dynamic

lkscale_trace: trace_dir
	python3 micro_trace_gen.py $(FISSLOCK_TRACE_PATH) $(HOST_NUM) lock-scale

trace_dir:
	mkdir -p $(FISSLOCK_TRACE_PATH)