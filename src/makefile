CC = g++
FLAGS = -std=c++11 `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp

SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:%.cpp=%.o)

test: $(OBJS)
	$(CC) -o $@ $(OBJS) $(FLAGS)

%.o: %.cpp 
	$(CC) $< -c

clean:
	rm *.o
	rm test
