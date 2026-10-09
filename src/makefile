CC = g++
# Every subdirectory is an include path, so a header is included by its bare
# name wherever it lives (#include "Stage.h")
DIRS = $(shell find . -type d)
# The creature testing tool lives in ../tools/creature_testing (its sources use the engine's
# headers by name too)
TOOL_DIR = ../tools/creature_testing
CXXFLAGS = -std=c++11 -g -Wall -pthread $(addprefix -I,$(DIRS)) -I$(TOOL_DIR)
LIBS = `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp -ldl -lpthread -lm

# macOS (MacPorts in /opt/local): no libGL/Vulkan, OpenGL is a framework
ifeq ($(shell uname),Darwin)
PREFIX = /opt/local
CXXFLAGS += -I$(PREFIX)/include -DGL_SILENCE_DEPRECATION
LIBS = -L$(PREFIX)/lib `pkg-config --static --libs glfw3` -lGLEW -framework OpenGL -lassimp -ldl -lpthread -lm
# The MacPorts ld cannot read the .tbd files of the newest SDK ("malformed file"): use an older one
OLD_SDK = /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
ifneq ($(wildcard $(OLD_SDK)),)
export SDKROOT ?= $(OLD_SDK)
endif
endif

# The three programs: the game's server (server.cpp is its main: it rules the world, with no
# window), the game's client (client.cpp: draws it and takes the player's input) and the creature
# testing tool (all of ../tools/creature_testing); everything else in src/ is shared
SRCS = $(shell find . -name '*.cpp')
SHARED_OBJS = $(filter-out ./client.o ./server.o,$(SRCS:%.cpp=%.o))
TOOL_SRCS = $(wildcard $(TOOL_DIR)/*.cpp)
TOOL_OBJS = $(TOOL_SRCS:%.cpp=%.o)

all: server client creature_testing

# The binaries go to ../test/; they can be run from any directory
server: server.o $(SHARED_OBJS)
	mkdir -p ../test
	$(CC) -o ../test/$@ $^ $(LIBS) -g

client: client.o $(SHARED_OBJS)
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
	rm -f ../test/server ../test/client ../test/test ../test/creature_testing

.PHONY: all clean server client creature_testing
