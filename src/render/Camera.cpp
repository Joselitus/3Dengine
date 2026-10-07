#include "Camera.h"
#include <iostream>

using namespace glm;

Camera::Camera(GLFWwindow * window, Shader * shader) {
	this->window = window;
	this->shader = shader;

	// Set vectors
	this->position = vec3(0.0f, 0.0f, 0.0f);
	this->rotation = vec2(0.0f, 0.0f);

	// Set matrices (before resize(), which uploads them all)
	this->view = mat4(1.0f);
	this->model = mat4(1.0f);

	// Resize window
	this->screenWidth = this->screenHeight = 0;
	this->resize();
}

void Camera::resize() {
	int width, height;
	glfwGetFramebufferSize(this->window, &width, &height);
	if (this->screenHeight != height || this->screenWidth != width) {
		std::cout << width << ", " << height << std::endl;
		glViewport(0, 0, width, height);
		this->screenHeight = height;
		this->screenWidth = width;
		this->setFov(this->fov);
	}
}

void Camera::setFov(float degrees) {
	this->fov = degrees;
	float aspect = this->screenHeight > 0 ? (float)this->screenWidth / this->screenHeight : 1.0f;
	this->projection = perspective(radians(degrees), aspect, 0.1f, farPlane);
	this->update();
}

void Camera::move(float x, float y, float z) {
	vec2 dir = glm::rotate(vec2(x, z), this->rotation.x);
	this->position += SPEED*vec3(dir.x, y, dir.y);
	this->view = translate(mat4(1.0f), this->position*-1.0f);
	this->update();
}

void Camera::reposition(float x, float y, float z) {
	this->view = translate(mat4(1.0f), vec3(-x, -y, -z));
	this->position = vec3(x, y, z);
	this->update();
}

void Camera::setAngles(float yaw, float pitch) {
	this->model = glm::rotate(mat4(1.0), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	this->model = glm::rotate(this->model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	this->model = this->model * this->base;
	this->rotation = vec2(yaw, pitch);
	this->update();
}

void Camera::setCarrier(const glm::mat3 &carrier) {
	// the view rotation is the inverse of the carrier's (a rotation: its transpose)
	this->base = mat4(glm::transpose(carrier));
	this->setAngles(this->rotation.x, this->rotation.y);
}

void Camera::attachTo(GameObject *target, float distance, float height) {
	this->target = target;
	this->distance = distance;
	this->height = height;
	if (this->base != mat4(1.0f)) { // nothing carries it any more
		this->base = mat4(1.0f);
		this->setAngles(this->rotation.x, this->rotation.y);
	}
	this->follow();
}

void Camera::follow() {
	if (!this->target)
		return;
	// World-space direction the camera is looking in (inverse of the rotation)
	vec3 forward = vec3(glm::inverse(this->model) * vec4(0.0f, 0.0f, -1.0f, 0.0f));
	this->position = this->target->getPosition() + vec3(0.0f, this->height, 0.0f)
		- forward * this->distance;
	this->view = translate(mat4(1.0f), this->position * -1.0f);
	this->update();
}

void Camera::update() {
	this->shader->setMatrix4("projection", value_ptr(this->projection));
	this->shader->setMatrix4("view", value_ptr(this->view));
	this->shader->setMatrix4("model", value_ptr(this->model));
	this->shader->setVector3("viewPosition", this->position.x, this->position.y, this->position.z);
}
