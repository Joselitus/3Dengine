#include "Shader.h"
#include "RenderStats.h"

Shader::Shader(const char* vertexShaderFile, const char* fragmentShaderFile) {
	this->ID = initializeShaders(vertexShaderFile, fragmentShaderFile);
}

int Shader::location(const char * name) {
	auto found = locations.find(name);
	if (found != locations.end())
		return found->second;
	int where = glGetUniformLocation(this->ID, name);
	locations[name] = where;
	return where;
}

void Shader::use() {
	glUseProgram(this->ID);
}

void Shader::setBool(const char * name, bool value) {
	RenderStats::uniformCalls()++;
	glUniform1i(location(name), (int)value);
}  

void Shader::setInt(const char * name, int value) {
	RenderStats::uniformCalls()++;
	glUniform1i(location(name), value);
}  

void Shader::setFloat(const char * name, float value) {
	RenderStats::uniformCalls()++;
	glUniform1f(location(name), value);
}

void Shader::setVector2(const char * name, float x, float y) {
	glUniform2f(glGetUniformLocation(this->ID, name), x, y);
}

void Shader::setVector3(const char * name, float x, float y, float z) {
	RenderStats::uniformCalls()++;
	glUniform3f(location(name), x, y, z);
}


void Shader::setMatrix4(const char * name, float * matrix) {
	RenderStats::uniformCalls()++;
	glUniformMatrix4fv(location(name), 1, GL_FALSE, matrix);
}