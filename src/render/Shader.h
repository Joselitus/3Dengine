#ifndef SHADER_H
#define SHADER_H

#include "myopengl.h"
  

// A linked GLSL program (vertex + fragment shader loaded from files).
// The constructor leaves it in use (glUseProgram) and the engine only has one,
// so the setters assume it is the current program.
class Shader
{
private:
    // the program ID
    unsigned int ID;
  
public:
    // constructor reads and builds the shader
    Shader(const char* vertexShaderFile, const char* fragmentShaderFile);
    // get shader id
    unsigned int getID() {return this->ID;}
    // use/activate the shader
    void use();
    // utility uniform functions
    void setBool(const char * name, bool value);  
    void setInt(const char * name, int value);   
    void setFloat(const char * name, float value);
    void setVector2(const char * name, float x, float y);
    void setVector3(const char * name, float x, float y, float z);
    void setMatrix4(const char * name, float * matrix);
};
  
#endif