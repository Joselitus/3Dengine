#ifndef CAMERA
#define CAMERA

#include "myopengl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Shader.h"

#define SENSIVILITY 0.005f
#define SPEED 0.07f

class Camera {
private:
	GLFWwindow * window;
	Shader * shader;

	int screenWidth;
	int screenHeight; 

	glm::vec3 position;
	glm::vec2 rotation;

	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;

public:
	Camera(GLFWwindow * window, Shader * shader);
	void resize();
	void move(float x, float y, float z);
	void rotate(int phi, int theta);
	void reposition(float x, float y, float z);
	void update();
	int getPhi();
	int getTheta();

};


#endif