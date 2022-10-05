#ifndef SKELETON
#define SKELETON

#include "myopengl.h"
#include "AnimatedMesh.h"
#include "Animation.h"

#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define MAX_BONES 100

class Bone;

class Skeleton {
     public:
     std::vector<Bone> bones;
     glm::mat4 globalInverseTransform;
     std::vector<glm::mat4> boneMats;
 
    Skeleton();

    Skeleton(std::vector<Bone> in_bones, glm::mat4 in_globalInverseTransform);
    void Init(std::vector<Bone> in_bones, glm::mat4 in_globalInverseTransform);
    Bone* FindBone(std::string name);
    void UpdateBoneMatsVector();
    void Update();
};

class Bone {
private:
     std::string name;    //The bone's name as loaded by ASSIMP.
     unsigned int id;     //The bone's index in the bone list.

     AnimatedMesh* mesh;
     aiNode* node;
     aiNodeAnim* animNode;
     Bone* parent_bone;    
     glm::mat4 parent_transforms;    
     glm::mat4 offset_matrix;    
     Skeleton* parent_skeleton; 

     //Keyframe Data
     glm::vec3 pos;
     glm::quat rot;
     glm::vec3 scale;
     glm::vec3 p1;
     glm::vec3 p2; 

public:
     Bone(){name = ""; id = -2;}
     Bone(AnimatedMesh* in_mesh, unsigned int in_id, std::string in_name, aiMatrix4x4 in_o_mat);
     Bone(AnimatedMesh* in_mesh, unsigned int in_id, std::string in_name, glm::mat4 in_o_mat);

     unsigned int FindPosition(float time);
     glm::vec3 CalcInterpolatedPosition(float time);
     unsigned int FindRotation(float time);
     glm::quat CalcInterpolatedRotation(float time);
     void UpdateKeyframeTransform(float time);

     void setAnimNode(aiNodeAnim * animNode) {this->animNode = animNode;}
     void setNode(aiNode * node) {this->node = node;}
     void setParentBone(Bone * bone) {this->parent_bone = bone;}
     void setParentSkeleton(Skeleton * skeleton) {this->parent_skeleton = skeleton;}

     aiNodeAnim * getAnimNode() {return this->animNode;}
     aiNode * getNode() {return this->node;}
     std::string getName() {return this->name;}
     glm::mat4 getOffsetMatrix() {return this->offset_matrix;}
  
     glm::mat4 GetParentTransforms();
};

#endif