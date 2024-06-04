
UTIL_SRC := $(shell find $(UTIL_DIR) -name *.c)
UTIL_CPP_SRC := $(shell find $(UTIL_DIR) -name *.cc)
LIB_OBJ += $(patsubst %.c, %.c.o, $(UTIL_SRC)) \
					 $(patsubst %.cc, %.cpp.o, $(UTIL_CPP_SRC))

$(UTIL_DIR)/%.c.o: $(UTIL_DIR)/%.c $(UTIL_DIR)/%.h $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(UTIL_DIR)/%.cpp.o: $(UTIL_DIR)/%.cc $(UTIL_DIR)/%.h $(HEADERS)
	$(CXX) -std=c++17 $(CFLAGS) -c $< -o $@