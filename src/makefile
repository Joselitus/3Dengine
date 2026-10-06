CC = g++
# Every subdirectory is an include path, so a header is included by its bare
# name wherever it lives (#include "Stage.h")
DIRS = $(shell find . -type d)
# The creature testing tool lives in ../tools/creature_testing (its sources use the engine's
# headers by name too)
TOOL_DIR = ../tools/creature_testing
CXXFLAGS = -std=c++11 -g -Wall -pthread $(addprefix -I,$(DIRS)) -I$(TOOL_DIR)
LIBS = `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp -ldl -lpthread -lm

# The two programs: the game (test.cpp is its main) and the creature testing tool (all of
# ../tools/creature_testing); everything else in src/ is shared
SRCS = $(shell find . -name '*.cpp')
SHARED_OBJS = $(filter-out ./test.o,$(SRCS:%.cpp=%.o))
TOOL_SRCS = $(wildcard $(TOOL_DIR)/*.cpp)
TOOL_OBJS = $(TOOL_SRCS:%.cpp=%.o)

all: test creature_testing

# The binaries go to ../test/; they can be run from any directory
test: test.o $(SHARED_OBJS)
	mkdir -p ../test
	$(CC) -o ../test/$@ $^ $(LIBS) -g

creature_testing: $(TOOL_OBJS) $(SHARED_OBJS)
	mkdir -p ../test
	$(CC) -o ../test/$@ $^ $(LIBS) -g

%.o: %.cpp
	$(CC) $(CXXFLAGS) $< -c -o $@ -MMD -MP

# Third-party code (stb_image, miniaudio): no warnings for it
third_party/%.o: third_party/%.cpp
	$(CC) $(CXXFLAGS) -w $< -c -o $@ -MMD -MP

-include $(SRCS:%.cpp=%.d) $(TOOL_SRCS:%.cpp=%.d)

clean:
	find . -name '*.o' -delete
	find . -name '*.d' -delete
	find $(TOOL_DIR) -name '*.o' -delete -o -name '*.d' -delete
	rm -f ../test/test ../test/creature_testing

.PHONY: all clean test creature_testing
