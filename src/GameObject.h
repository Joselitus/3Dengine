#ifndef GAME_OBJECT
#define GAME_OBJECT

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
public:
	GameObject(Model * model);
	GameObject(Model * model, float x, float y, float z);
	virtual void UpdateSkeleton();
	void setPosition(float x, float y, float z) {this->position = glm::vec3(x, y, z);}
	void Draw(Shader * shader);
};


#endif