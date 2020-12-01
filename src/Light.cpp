#include "Light.h"
using namespace glm;

Light::Light(float r, float g, float b, Shader * shader) {
	this->shader = shader;
	this->color = vec3(r, g, b);
	this->position = vec3(0, 0, 0);
	this->update();
}
void Light::moveTo(float x, float y, float z) {
	this->position = vec3(x, y, z);
	this->update();
}
void Light::update() {
	this->shader->setVector3("lightPosition", this->position.x, this->position.y, this->position.z);
	this->shader->setVector3("lightColor", this->color.x, this->color.y, this->color.z);
}