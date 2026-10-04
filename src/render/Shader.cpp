#include "Shader.h"

Shader::Shader(const char* vertexShaderFile, const char* fragmentShaderFile) {
	this->ID = initializeShaders(vertexShaderFile, fragmentShaderFile);
}

void Shader::use() {
	glUseProgram(this->ID);
}

void Shader::setBool(const char * name, bool value) {
	glUniform1i(glGetUniformLocation(this->ID, name), (int)value);
}  

void Shader::setInt(const char * name, int value) {
	glUniform1i(glGetUniformLocation(this->ID, name), value);
}  

void Shader::setFloat(const char * name, float value) {
	glUniform1f(glGetUniformLocation(this->ID, name), value);
}

void Shader::setVector3(const char * name, float x, float y, float z) {
	glUniform3f(glGetUniformLocation(this->ID, name), x, y, z);
}


void Shader::setMatrix4(const char * name, float * matrix) {
	glUniformMatrix4fv(glGetUniformLocation(this->ID, name), 1, GL_FALSE, matrix);
}