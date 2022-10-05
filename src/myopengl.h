#ifndef MYOPENGL
#define MYOPENGL

#include "stb_image.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <assimp/scene.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#define TEXTURES_INVERTED 0

char * fileToString(const char * file);

unsigned int TextureFromFile(const char * name, std::string directory);

aiMatrix4x4 GLMMat4ToAi(glm::mat4 mat);

glm::mat4 AiToGLMMat4(aiMatrix4x4& in_mat);

unsigned int initializeShaders(const char * vertexShaderSource, const char * fragmentShaderSource);

void initializeVertexBuffers(float * vertices, int lenght, int atributes);

unsigned int initializeIndexBuffers(unsigned int * index, int lenght);

unsigned int bindNewVAO();

unsigned int newVAO(float * vertices, int lenght, int * atribute_sizes, int atributes);

unsigned int newIndexVAO(float * vertices, int lenght, int * atribute_sizes, int atributes, unsigned int * indices, int lenght_indices);

unsigned int loadTexture(const char * path);

#endif
