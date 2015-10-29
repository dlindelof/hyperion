.PHONY: all
all: tests valgrind_tests

BUILD_DIR      := build
TEST_BUILD_DIR := $(BUILD_DIR)/tests

SRC_DIRS      := .
UTILITIES_DIR := utilities
TEST_SRC_DIR  := tests


# source files
CSRCS   	:= $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
UTILITIES_CSRCS := $(notdir $(foreach dir, $(UTILITIES_DIR), $(wildcard $(dir)/*.c)))
ALLSRCS      	:= $(CSRCS)
TEST_CPPSRCS 	:= $(wildcard $(TEST_SRC_DIR)/*.cpp)

# targets
$(TEST_BUILD_DIR) $(BUILD_DIR):
	mkdir -p $@

# object files
COBJS        := $(CSRCS:%.c=$(BUILD_DIR)/%.o)
UTILITIES_COBJS := $(UTILITIES_CSRCS:%.c=$(BUILD_DIR)/%.o)
TEST_CPPOBJS := $(patsubst $(TEST_SRC_DIR)/%.cpp,$(TEST_BUILD_DIR)/%.o,$(TEST_CPPSRCS))

# common flags
CPPFLAGS += -g -Wall -Wextra -Wno-unused -Wno-unused-parameter -Werror -I.
CFLAGS += -std=c99
CXXFLAGS := 

$(COBJS): $(BUILD_DIR)/%.o: $(SRC_DIRS)/%.c | $(BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
	
$(UTILITIES_COBJS): $(BUILD_DIR)/%.o: $(UTILITIES_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
	
$(TEST_CPPOBJS): $(TEST_BUILD_DIR)/%.o: $(TEST_SRC_DIR)/%.cpp | $(TEST_BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<


lzss: $(COBJS) $(UTILITIES_COBJS) | $(BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -o $(BUILD_DIR)/lzss $(UTILITIES_COBJS) $(COBJS)


$(TEST_BUILD_DIR)/tests: CC = gcc
$(TEST_BUILD_DIR)/tests: CPPFLAGS += -g
$(TEST_BUILD_DIR)/tests: CPPFLAGS += -fprofile-arcs -ftest-coverage
$(TEST_BUILD_DIR)/tests: CXX = g++
$(TEST_BUILD_DIR)/tests: LD = ld
$(TEST_BUILD_DIR)/tests: LDLIBS += `pkg-config --libs cpputest` -lgcov
$(TEST_BUILD_DIR)/tests: $(CSRCS) $(TEST_CPPOBJS)
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -o $@ $(TEST_CPPOBJS) $(LDLIBS)


.PHONY: tests
tests: $(TEST_BUILD_DIR)/tests 
	$(TEST_BUILD_DIR)/tests

.PHONY: valgrind_tests
valgrind_tests: tests
	valgrind --error-exitcode=2 --leak-check=full --log-file=$(TEST_BUILD_DIR)/valgrind_tests.out $(TEST_BUILD_DIR)/tests
	
.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)
