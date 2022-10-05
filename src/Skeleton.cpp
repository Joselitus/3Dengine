#include "Skeleton.h"

Skeleton::Skeleton(std::vector<Bone> in_bones, glm::mat4 in_globalInverseTransform) {
     Init(in_bones,in_globalInverseTransform);
}

void Skeleton::Init(std::vector<Bone> in_bones, glm::mat4 in_globalInverseTransform) {
	bones = in_bones;
	globalInverseTransform = in_globalInverseTransform;

	for(int i = 0; i < bones.size(); i++)
	 bones.at(i).setParentSkeleton(this);
}

Bone* Skeleton::FindBone(std::string name) {
	 for(int i = 0; i < bones.size(); i++) {
	     if(bones.at(i).getName() == name)
	         return &bones.at(i);
	 }
	 return nullptr;
}

void Skeleton::UpdateBoneMatsVector() {
     //The purpose of this function is to gather all of the bones'
     //global transformations (which we calculate below) and place them, in order,
     //in a vector of glm::mat4s. This is necessary because we'll have to pass
     //these matrices into the shader later on, and this format makes that really
     //easy.

     //If there are no bones, why bother?
     if(bones.size() == 0)
         return;
 
     //Make sure there's nothning left in the vector.
     boneMats.clear();



     //Here we must update the matrices to the shader until the MAX_BONES constant
     //in the shader, which is 100. You could set this as an engine variable but
     //for the purposes of this tutorial simply typing i < 100 will do.
     //We do this in order to make certain that every index in the shader's array
     //of matrices is filled, that there are no garbage or left-over matrices from
     //some other draw call.
     for(int i = 0; i < MAX_BONES; i++)
     {
         //If we are past the number of bones in the actual skeleton, we simply
         //pass in an identity matrix.
         if(i > bones.size() - 1)
             boneMats.push_back(glm::mat4(1.0));
         else
         {
         //Now for the complicated stuff...
         //To get the concatenated transformation, we multiply the concatenated transforms of
         //the bone's parents with the bone's transformation.
         glm::mat4 concatenated_transformation = (bones.at(i).GetParentTransforms() * 
                                            AiToGLMMat4(bones.at(i).getNode()->mTransformation));
         
         //DISCLAIMER! By concatenated_transformation, what I actually mean is the transformation of the current bone in the
//skeleton's local (or object) space. At this point it is not in world space, as being in world space requires multiplication by the
//model matrix, which happens only in the shader stage.

         //REMEMBER: TO UNDERSTAND MATRIX MULTIPLICATION, WE MUST READ IT BACKWARDS!
         //Here we transform the bone back to the origin (with the offset matrix),
         //we multiply it by the concatenated transforms,
         //and then by the globalInverseTransform.
         boneMats.push_back(globalInverseTransform * concatenated_transformation* bones.at(i).getOffsetMatrix());
         }
    }
 }

void Skeleton::Update() {
     //This is the function where we'll update all of the animated transformations 
     UpdateBoneMatsVector();
}

//This here is a simple constructor, which merely sets the member data
//to that which was passed in. Fancy, I know.
Bone::Bone(AnimatedMesh* in_mesh, unsigned int in_id, std::string in_name, aiMatrix4x4 in_o_mat)
{
     id = in_id;
     name = in_name;
     offset_matrix = AiToGLMMat4(in_o_mat);

     mesh = in_mesh;

     parent_bone = nullptr;
     node = nullptr;
}
//This constructor is similar to the last one in the sense that it is
//virtually identical and whose existence seems almost pointless. Almost.
Bone::Bone(AnimatedMesh* in_mesh, unsigned int in_id, std::string in_name, glm::mat4 in_o_mat)
{
     id = in_id;
     name = in_name;
     offset_matrix = in_o_mat;
 
     mesh = in_mesh;

     parent_bone = nullptr;
     node = nullptr;
}
//Finally! An actual function!
//This function simply concatenates the parents' transforms.
//This is vital for calculating this bone's final transformation.
glm::mat4 Bone::GetParentTransforms()
{
     Bone* b = parent_bone;    //In order to recursively concatenate the transforms,
                        //we first start with this bone's parent.
     std::vector<glm::mat4> mats;    //Where we'll store the concatenated transforms.

     while(b != nullptr)    //As long as 'b' has a parent (see the end of the loop
     {                      //to avoid confusion).
         glm::mat4 tmp_mat = AiToGLMMat4(b->node->mTransformation); //This bone's transformation.
         mats.push_back(tmp_mat);

         b = b->parent_bone;    //We set b to its own parent so the loop can continue.
     }

     glm::mat4 concatenated_transforms;
     //IMPORTANT!!!! This next loop must be done in reverse, 
     //as multiplication with matrices is not commutative.
     for(int i = mats.size()-1; i >= 0; i--)
         concatenated_transforms *= mats.at(i);

     return concatenated_transforms;    //Finally, we return the concatenated transforms.
}

//All of these Find functions return an integer because they aren't returning
//keyframes themselves, they are returning indices to the current keyframe
//within our stored array of keyframes, animNode.

unsigned int Bone::FindPosition(float time)
{
     for(unsigned int i = 0 ; i < animNode->mNumPositionKeys - 1 ; i++)
     {
         //If the time passed in is less than the time of the next
         //keyframe, then this is the keyframe we want!
         if(time < (float)animNode->mPositionKeys[i + 1].mTime)
             return i;
     }
     return 0;    //If we don't find anything, just return 0.
}

unsigned int Bone::FindRotation(float time)
{
     for(unsigned int i = 0 ; i < animNode->mNumRotationKeys - 1 ; i++)
     {
         //Same as with the position.
         if(time < (float)animNode->mRotationKeys[i + 1].mTime)
             return i;
     }
     return 0;
}


glm::vec3 Bone::CalcInterpolatedPosition(float time)
{
     //If there's only one keyframe for the position, we return it.
     //This avoids any possible out of range errors later on in the function
     //as we're dealing with an array.
     if(animNode->mNumPositionKeys == 1)
     {
         aiVector3D assimp_val = animNode->mPositionKeys[0].mValue;
         glm::vec3 val(assimp_val.x,assimp_val.y,assimp_val.z);
         return val;
     }

     //The index of our current position, as well as the index that follows.
     //This will allow us to interpolate between the two values.
     unsigned int PositionIndex = FindPosition(time);
     unsigned int NextPositionIndex = (PositionIndex + 1);

     //DeltaTime is the amount of time between the two keyframes.
     float DeltaTime = animNode->mPositionKeys[NextPositionIndex].mTime - animNode->mPositionKeys[PositionIndex].mTime;
     //The factor is simply how much time has passed since the last keyframe,
     //divided by the amount of time between the two keyframes (the DeltaTime)
     //to get the percentage, or how far along between the two keyframes we are.
     float Factor = (time - (float)animNode->mPositionKeys[PositionIndex].mTime) / DeltaTime;
 
     //The start and end positions (the position values of each of the keyframes)
     const aiVector3D StartPosition = animNode->mPositionKeys[PositionIndex].mValue;
     const aiVector3D EndPosition = animNode->mPositionKeys[NextPositionIndex].mValue;
 
     //Here we convert them to glm matrices...
     glm::vec3 p1(StartPosition.x,StartPosition.y,StartPosition.z);
     glm::vec3 p2(EndPosition.x,EndPosition.y,EndPosition.z);

 
     //and here we linearly interpolate between the two keyframes.
     glm::vec3 val = glm::mix(p1,p2,Factor);

     return val;
}

//This function is identical to the position one, except it's for rotation ...
//believe it or not.
glm::quat Bone::CalcInterpolatedRotation(float time)
{
     if(animNode->mNumRotationKeys == 1)
     {
         aiQuaternion assimp_val = animNode->mRotationKeys[0].mValue;
         glm::quat val(assimp_val.w,assimp_val.x,assimp_val.y,assimp_val.z);
         return val;
     }

     unsigned int RotationIndex = FindRotation(time);
     unsigned int NextRotationIndex = (RotationIndex + 1);

     float DeltaTime = animNode->mRotationKeys[NextRotationIndex].mTime - animNode->mRotationKeys[RotationIndex].mTime;
     float Factor = (time - (float)animNode->mRotationKeys[RotationIndex].mTime) / DeltaTime;

     const aiQuaternion& StartRotationQ = animNode->mRotationKeys[RotationIndex].mValue;
     const aiQuaternion& EndRotationQ = animNode->mRotationKeys[NextRotationIndex].mValue;
 
     glm::quat r1(StartRotationQ.w,StartRotationQ.x,StartRotationQ.y,StartRotationQ.z);
     glm::quat r2(EndRotationQ.w,EndRotationQ.x,EndRotationQ.y,EndRotationQ.z);

     glm::quat val = glm::slerp(r1,r2,Factor);
     return val;
}

//This function will update our node's transformation to match that of
//our keyframes.
void Bone::UpdateKeyframeTransform(float time)
{
     if(animNode == nullptr)
         return;

     pos = CalcInterpolatedPosition(time);
     rot = CalcInterpolatedRotation(time);
     scale = glm::vec3(1.0);    //You could add support for scaling, but that's
                                //beyond.
 

     glm::mat4 mat;
         mat *= glm::translate(glm::mat4(1.0), pos);
         mat *= glm::mat4_cast(rot);
         mat *= glm::scale(glm::mat4(1.0), scale);

     node->mTransformation = GLMMat4ToAi(mat);
}