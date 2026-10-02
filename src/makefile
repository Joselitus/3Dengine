CC = g++
FLAGS = -std=c++11 `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp

SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:%.cpp=%.o)

test: $(OBJS)
	$(CC) -o ../test/$@ $(OBJS) $(FLAGS) -g

%.o: %.cpp 
	$(CC) $< -c -g -MMD -MP

-include $(SRCS:%.cpp=%.d)

clean:
	rm *.o
	rm ../test/test
