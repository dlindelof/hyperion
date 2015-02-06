.PHONY: all
all: tests

CPPUTEST_INCLUDE := /usr/include/CppUTest
CPPUTEST_LIB     := /usr/lib/x86_64-linux-gnu/libCppUTest.a
CPPUTESTEXT_LIB  := /usr/lib/x86_64-linux-gnu/libCppUTestExt.a

BUILD_DIR      := build
TEST_BUILD_DIR := $(BUILD_DIR)/tests

SRC_DIRS     := .
TEST_SRC_DIR := tests

# source files
CSRCS   	:= $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
CPPSRCS 	:= $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))
ALLSRCS      	:= $(CSRCS) $(CPPSRCS)
TEST_CPPSRCS 	:= $(wildcard $(TEST_SRC_DIR)/*.cpp)

# object files
COBJS        := $(CSRCS:%.c=$(BUILD_DIR)/%.o)
CPPOBJS      := $(CPPSRCS:%.cpp=$(BUILD_DIR)/%.o)
TEST_CPPOBJS := $(patsubst $(TEST_SRC_DIR)/%.cpp,$(TEST_BUILD_DIR)/%.o,$(TEST_CPPSRCS))

# common flags
CPPFLAGS += -g -Wall -Wextra -Wno-unused -Wno-unused-parameter -Werror -I.
CFLAGS += -std=c99
CXXFLAGS := 

$(COBJS): $(ALLSRCS)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
	
$(CPPOBJS): $(ALLSRCS)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
	
ttt: $(TEST_CPPOBJS)
	echo $(COBJS)
	echo $(ALLSRCS)
	echo $(TEST_SRC_DIR)
	echo $(TEST_BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
	
	

# targets
$(TEST_BUILD_DIR):
	mkdir -p $@

$(TEST_BUILD_DIR)/tests: CC = gcc
$(TEST_BUILD_DIR)/tests: CPPFLAGS += -I$(CPPUTEST_INCLUDE) -g
$(TEST_BUILD_DIR)/tests: CPPFLAGS += -fprofile-arcs -ftest-coverage
$(TEST_BUILD_DIR)/tests: CXX = g++
$(TEST_BUILD_DIR)/tests: LD = ld
$(TEST_BUILD_DIR)/tests: LDLIBS += $(CPPUTEST_LIB) $(CPPUTESTEXT_LIB) -lgcov
$(TEST_BUILD_DIR)/tests: $(TEST_CPPOBJS)
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)


$(TEST_CPPOBJS): $(TEST_BUILD_DIR)/%.o: $(TEST_SRC_DIR)/%.cpp | $(TEST_BUILD_DIR)
	$(CC) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

.PHONY: tests
tests: $(TEST_BUILD_DIR)/tests 
	$(TEST_BUILD_DIR)/tests

.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)
