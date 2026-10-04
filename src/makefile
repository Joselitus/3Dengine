CC = g++
CXXFLAGS = -std=c++11 -g -Wall
LIBS = `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp

SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:%.cpp=%.o)

# The game binary goes to ../test/test; it can be run from any directory
test: $(OBJS)
	mkdir -p ../test
	$(CC) -o ../test/$@ $(OBJS) $(LIBS) -g

%.o: %.cpp
	$(CC) $(CXXFLAGS) $< -c -MMD -MP

# stb_image is third-party code: no warnings for it
stb_image.o: stb_image.cpp
	$(CC) $(CXXFLAGS) -w $< -c -MMD -MP

-include $(SRCS:%.cpp=%.d)

clean:
	rm -f *.o *.d ../test/test

.PHONY: clean
