
TATP_TXN_NUM := 1000000

all: main

main: main.cc tatp_lock_gen.cc
	$(CXX) main.cc tatp_lock_gen.cc -o main

traces: main
	@echo "Generating TATP traces for all $(HOST_NUM) hosts..."
	@./main $(HOST_NUM) $(TATP_TXN_NUM) $(FISSLOCK_TRACE_PATH)

clean:
	rm -rf main