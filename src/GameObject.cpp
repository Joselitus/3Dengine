#include "GameObject.h"
using namespace std;
using namespace glm;

GameObject::GameObject(Model * model) {
	this->model = model;
	Mesh * mesh = &(model->meshes[0]);
	if (dynamic_cast<AnimatedMesh*>(mesh)) {
		AnimatedMesh * animesh = (AnimatedMesh*)mesh;
		this->skeleton = animesh->skeleton;
		this->anim = true;
	}
	else {
		this->skeleton = nullptr;
		this->anim = false;
	}
}

GameObject::GameObject(Model * model, float x, float y, float z): GameObject(model) {
	this->position = vec3(x, y, z);
}

void GameObject::UpdateSkeleton() {

}

void GameObject::Draw(Shader * shader) {
	if(this->anim) {
		this->UpdateSkeleton(); 
		glUniformMatrix4fv(glGetUniformLocation(shader->getID(),"gBones"),
        this->skeleton->boneMats.size(),
        GL_FALSE,  
        glm::value_ptr(this->skeleton->boneMats[0]));
	}
	shader->setVector3("objposition", position.x, position.y, position.z);
	this->model->Draw(shader);
}