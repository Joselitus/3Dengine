CC = g++
FLAGS = -std=c++11 `pkg-config --static --libs glfw3` -lvulkan -lGLEW -lGL -lassimp

test: test.cpp myopengl.h myopengl.cpp Shader.h Shader.cpp stb_image.h stb_image.cpp Mesh.h Mesh.cpp AnimatedMesh.h AnimatedMesh.cpp Model.h Model.cpp AnimatedModel.h AnimatedModel.cpp Skeleton.h Skeleton.cpp Animation.h Animation.cpp Light.h Light.cpp GameObject.h GameObject.cpp
	$(CC) -o $@ test.cpp myopengl.cpp Mesh.cpp AnimatedMesh.cpp Model.cpp AnimatedModel.cpp Skeleton.cpp Animation.cpp Shader.cpp Camera.cpp Light.cpp GameObject.cpp stb_image.cpp $(FLAGS)


clean:
	rm test