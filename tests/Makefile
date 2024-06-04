
#####################################################################
# Microbenchmark
MICROBENCH_SRC := $(TESTS_DIR)/microbench.cc
MICROBENCH_BIN := $(BUILD_DIR)/microbench_$(SYSTEM)

$(MICROBENCH_BIN): $(MICROBENCH_SRC) $(LIB_OBJ) $(R2_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(CFLAGS) -std=c++17 $(LDFLAGS)

microbench: $(MICROBENCH_BIN)
	
#####################################################################
# Transaction processing benchmark
TXNBENCH_SRC := $(TESTS_DIR)/txnbench.cc
TXNBENCH_BIN := $(BUILD_DIR)/txnbench_$(SYSTEM)

$(TXNBENCH_BIN): $(TXNBENCH_SRC) $(LIB_OBJ) $(R2_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(CFLAGS) -std=c++17 $(LDFLAGS)

txnbench: $(TXNBENCH_BIN)

#####################################################################
# Redis-backed application test
REDLOCK_DIR := ./vendor/redlock-cpp
REDLOCK_OBJ := $(REDLOCK_DIR)/bin/libredlock.a
HIREDIS_DIR := ./vendor/hiredis
HIREDIS_OBJ := $(HIREDIS_DIR)/libhiredis.a

REDIS_TXN_SRC := $(TESTS_DIR)/redisapp.cc
REDIS_TXN_BIN := $(BUILD_DIR)/redis_txn_$(SYSTEM)

$(REDLOCK_OBJ): $(REDLOCK_DIR)/redlock-cpp/*
	make -C $(REDLOCK_DIR) -j

$(HIREDIS_OBJ): $(HIREDIS_DIR)/*.c
	make -C $(HIREDIS_DIR) -j

$(REDIS_TXN_BIN): $(REDIS_TXN_SRC) $(LIB_OBJ) \
									$(R2_OBJ) $(HIREDIS_OBJ) $(REDLOCK_OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ $(CFLAGS) -I$(HIREDIS_DIR) -I$(REDLOCK_DIR) -o $@ \
		-std=c++17 $(LDFLAGS)

redisapp: $(REDIS_TXN_BIN)

#####################################################################
# The test list
tests: microbench txnbench redisapp
