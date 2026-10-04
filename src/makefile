CC = g++
CXXFLAGS = -std=c++11 -g -Wall -pthread
LIBS = `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp -ldl -lpthread -lm

SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:%.cpp=%.o)

# The game binary goes to ../test/test; it can be run from any directory
test: $(OBJS)
	mkdir -p ../test
	$(CC) -o ../test/$@ $(OBJS) $(LIBS) -g

%.o: %.cpp
	$(CC) $(CXXFLAGS) $< -c -MMD -MP

# Third-party code (stb_image, miniaudio): no warnings for it
stb_image.o: stb_image.cpp
	$(CC) $(CXXFLAGS) -w $< -c -MMD -MP
miniaudio.o: miniaudio.cpp
	$(CC) $(CXXFLAGS) -w $< -c -MMD -MP

-include $(SRCS:%.cpp=%.d)

clean:
	rm -f *.o *.d ../test/test

.PHONY: clean
