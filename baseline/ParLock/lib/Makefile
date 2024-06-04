
LIB_SRC := $(shell find $(LIB_DIR) -name *.c)
LIB_CPP_SRC := $(shell find $(LIB_DIR) -name *.cc)
LIB_OBJ += $(patsubst %.c, %.c.o, $(LIB_SRC)) \
					 $(patsubst %.cc, %.cpp.o, $(LIB_CPP_SRC))

$(LIB_DIR)/%.c.o: $(LIB_DIR)/%.c $(LIB_DIR)/%.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_DIR)/%.cpp.o: $(LIB_DIR)/%.cc $(LIB_DIR)/%.h
	$(CXX) -std=c++17 $(CFLAGS) -c $< -o $@