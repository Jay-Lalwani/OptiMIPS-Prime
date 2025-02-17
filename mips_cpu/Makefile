CXX = g++
CXXFLAGS= -g -Wall -std=c++11 -DENABLE_DEBUG
OPTFLAGS= -O3

EXE_NAME=processor
SRCS := main.cpp memory.cpp processor.cpp
OBJS := $(SRCS:.cpp=.o)

# Get all test directories
TEST_DIRS := $(wildcard test_data_pipeline/*)

.PHONY: all clean test

all: $(EXE_NAME)

$(EXE_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

processor.o: regfile.h ALU.h control.h processor.h
memory.o: memory.h
main.o: memory.h processor.h

test: $(EXE_NAME)
	@for dir in $(TEST_DIRS); do \
		if [ -f $$dir/test.s ]; then \
			echo "Testing $$(basename $$dir)..."; \
			mips-linux-gnu-gcc -mips32 -EL -static $$dir/test.s -nostartfiles -Ttext=0 -o test.bin; \
			./$(EXE_NAME) --bmk=test.bin -O0 > logs/single_cycle/$$(basename $$dir).txt; \
			./$(EXE_NAME) --bmk=test.bin -O1 > logs/pipeline/$$(basename $$dir).txt; \
		fi \
	done
	@rm -f test.bin

clean:
	$(RM) $(EXE_NAME) $(OBJS) test.bin


