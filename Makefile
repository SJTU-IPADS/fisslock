BASELINE_DIR := ./baseline
NETLOCK_DIR := $(BASELINE_DIR)/NetLock
NETLOCK_SERVER_DIR := $(BASELINE_DIR)/NetLock/server
PARLOCK_DIR := $(BASELINE_DIR)/ParLock

# Common dirs
BUILD_DIR := ./build
TESTS_DIR := ./tests
APP_DIR := ./app
UTIL_DIR := ./utils
INC_DIR := ./include

# System specific dirs
SYSTEM ?= fisslock
ifeq ($(SYSTEM), netlock)
	LIB_DIR := $(NETLOCK_DIR)/lib
else ifeq ($(SYSTEM), srvlock)
	LIB_DIR := $(NETLOCK_DIR)/lib
else ifeq ($(SYSTEM), parlock)
	LIB_DIR := $(PARLOCK_DIR)/lib
else
	LIB_DIR := ./lib
endif

ifeq ($(SYSTEM), netlock)
CFLAGS += -DNETLOCK
else ifeq ($(SYSTEM), srvlock)
CFLAGS += -DNETLOCK
endif

RIB_DIR := ./vendor/rib
R2_DIR := ./vendor/r2

R2_DEPS_DIR := $(R2_DIR)/deps
R2_SRC_DIR := $(R2_DIR)/src
BOOST_INC_DIR := $(R2_DEPS_DIR)/boost/include
BOOST_LIB_DIR := $(R2_DEPS_DIR)/boost/lib

R2_OBJ := $(R2_DIR)/libr2.a
R2_OBJ += $(BOOST_LIB_DIR)/libboost_coroutine.a
R2_OBJ += $(BOOST_LIB_DIR)/libboost_thread.a
R2_OBJ += $(BOOST_LIB_DIR)/libboost_context.a
R2_OBJ += $(BOOST_LIB_DIR)/libboost_system.a

DEBUG ?= 1
PKGCONF ?= pkg-config

CC ?= gcc
CFLAGS += -fPIC
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -DNO_DCT
LDFLAGS += -libverbs -lpthread -ldl
LDFLAGS += $(shell $(PKGCONF) --libs libdpdk)

CFLAGS += -I$(INC_DIR) -I$(UTIL_DIR) -I$(LIB_DIR) -I$(RIB_DIR) -I$(TOML_DIR) 
CFLAGS += -I$(R2_DEPS_DIR)
CFLAGS += -I$(R2_SRC_DIR)
CFLAGS += -I$(BOOST_INC_DIR)

ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif

ifeq ($(SYSTEM), netlock)
all: tests server
else ifeq ($(SYSTEM), srvlock)
all: tests server
else
all: tests
endif

include $(INC_DIR)/Makefile
include $(LIB_DIR)/Makefile
include $(UTIL_DIR)/Makefile
include $(TESTS_DIR)/Makefile

.PRECIOUS: $(LIB_OBJ)

ifeq ($(SYSTEM), netlock)
include $(NETLOCK_SERVER_DIR)/Makefile
server: $(NETLOCK_SERVER_BIN)
else ifeq ($(SYSTEM), srvlock)
include $(NETLOCK_SERVER_DIR)/Makefile
server: $(NETLOCK_SERVER_BIN)
endif

clean:
	rm -rf build $(LIB_OBJ) $(NETLOCK_SERVER_BIN)
