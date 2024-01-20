#ifndef GAME_OBJECT
#define GAME_OBJECT

#include <glm/ext/matrix_float4x4.hpp>
#include <vector>

#include <glm/glm.hpp>

#include "AnimatedMesh.h"
#include "AnimatedModel.h"
#include "Skeleton.h"

class GameObject {
private:
	bool anim;
	Model * model;
	Skeleton * skeleton;
	glm::vec3 position;
	glm::mat4 rotation;
public:
	GameObject(Model * model);
	GameObject(Model * model, float x, float y, float z);
	virtual void UpdateSkeleton();
	void setPosition(float x, float y, float z) {this->position = glm::vec3(x, y, z);}
	void setRotation(glm::mat4 rotation) {this->rotation = rotation;}
	void Draw(Shader * shader);
};


#endif
