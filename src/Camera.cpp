#include "Camera.h"
#include <iostream>

using namespace glm;

Camera::Camera(GLFWwindow * window, Shader * shader) {
	this->window = window;
	this->shader = shader;
	// Resize window
	this->screenWidth = this->screenHeight = 0;
	this->resize();
	
	// Set vectors
	this->position = vec3(0.0f, 0.0f, 0.0f);
	this->rotation = vec2(0.0f, 0.0f);

	// Set matrices
	this->view = mat4(1.0f);
	this->model = mat4(1.0f);
}

void Camera::resize() {
	int width, height;
	// TODO (fix this)
	glfwGetWindowSize(this->window, &width, &height);
	if (this->screenHeight != height || this->screenWidth != width) {
		std::cout << width << ", " << height << std::endl;
		float fwidth = (float)width;
		float fheight = (float)height;
		this->projection = perspective(radians(45.0f), fwidth/fheight, 0.1f, 100.0f);
		this->screenHeight = height;
		this->screenWidth = width;
	}
}

void Camera::move(float x, float y, float z) {
	vec2 dir = glm::rotate(vec2(x, z), SENSIVILITY*this->rotation.x);
	this->position += SPEED*vec3(dir.x, y, dir.y);
	this->view = translate(mat4(1.0f), this->position*-1.0f);
	this->update();
}

void Camera::reposition(float x, float y, float z) {
	this->view = translate(mat4(1.0f), vec3(-x, -y, -z));
	this->position = vec3(x, y, z);
	this->update();
}

void Camera::rotate(int phi, int theta) {
	this->model = glm::rotate(mat4(1.0), (float)SENSIVILITY*theta, glm::vec3(1.0f, 0.0f, 0.0f));
	this->model = glm::rotate(this->model, (float)SENSIVILITY*phi, glm::vec3(0.0f, 1.0f, 0.0f));
	this->rotation = vec2(phi, theta);
	this->update();
}

void Camera::update() {
	this->shader->setMatrix4("projection", value_ptr(this->projection));
	this->shader->setMatrix4("view", value_ptr(this->view));
	this->shader->setMatrix4("model", value_ptr(this->model));
	this->shader->setVector3("viewPosition", this->position.x, this->position.y, this->position.z);
}

int Camera::getPhi() {
	return this->rotation.x;
}

int Camera::getTheta() {
	return this->rotation.y;
}