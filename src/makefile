CC = g++
# Every subdirectory is an include path, so a header is included by its bare
# name wherever it lives (#include "Stage.h")
DIRS = $(shell find . -type d)
CXXFLAGS = -std=c++11 -g -Wall -pthread $(addprefix -I,$(DIRS))
LIBS = `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp -ldl -lpthread -lm

SRCS = $(shell find . -name '*.cpp')

OBJS = $(SRCS:%.cpp=%.o)

# The game binary goes to ../test/test; it can be run from any directory
test: $(OBJS)
	mkdir -p ../test
	$(CC) -o ../test/$@ $(OBJS) $(LIBS) -g

%.o: %.cpp
	$(CC) $(CXXFLAGS) $< -c -o $@ -MMD -MP

# Third-party code (stb_image, miniaudio): no warnings for it
third_party/%.o: third_party/%.cpp
	$(CC) $(CXXFLAGS) -w $< -c -o $@ -MMD -MP

-include $(SRCS:%.cpp=%.d)

clean:
	find . -name '*.o' -delete
	find . -name '*.d' -delete
	rm -f ../test/test

.PHONY: clean
