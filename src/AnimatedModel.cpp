#include "AnimatedModel.h"
using namespace std;
using namespace glm;

AnimatedModel::AnimatedModel(const char * path): Model(path) {}

void AnimatedModel::loadModel(string path) {
	Assimp::Importer import;
	scene = import.ReadFile(path, aiProcess_GenNormals | aiProcess_Triangulate | aiProcess_FlipUVs);

	if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        cout << "ERROR::ASSIMP::" << import.GetErrorString() << endl;
        return;
    }
  directory = path.substr(0, path.find_last_of('/'));
  rootNode = scene->mRootNode;
  recursiveNodeProcess(rootNode);
  AnimNodeProcess();
	globalInverseTransform = inverse(AiToGLMMat4(rootNode->mTransformation));

  this->processNode(rootNode, scene);


	for(int i = 0; i < scene->mNumMeshes; i++) {
	    for(int j = 0; j < scene->mMeshes[i]->mNumBones; j++) {
	         //Here we're just storing the bone information that we loaded
	         //with ASSIMP into the formats our Bone class will recognize.
	         string b_name = scene->mMeshes[i]->mBones[j]->mName.data;
	         mat4 b_mat = transpose(AiToGLMMat4(scene->mMeshes[i]->mBones[j]->mOffsetMatrix));
	         
	         //Just because I like debugging...
	         //std::cout<<"Bone "<<j<<" "<<b_name<<std::endl;
	         
	         //Here we create a Bone Object with the information we've
	         //gathered so far, but wait, there's more!
	         Bone bone(&(meshes.at(i)),i,b_name,b_mat);
	         
	         //These next parts are simple, we just fill up the bone's
	         //remaining data using the functions we defined earlier.
	         bone.setNode(FindAiNode(b_name));
	         bone.setAnimNode(FindAiNodeAnim(b_name));

	         if(bone.getNode() == nullptr)
	             std::cout<<"No Animations were found for "+b_name<<std::endl;
	         
	         //Finally, we push the Bone into our vector. Yay.
	         bones.push_back(bone);
	     }
	 }


	 //Now we have to fill up the remaining ... remaining data within the
	 //bone object, specifically: the pointers to the bone's parent bone.
	 for(int i = 0; i < bones.size(); i++)
	 {
	     //Here we cycle through the existing bones and match them up with
	     //their parents, the code here is pretty self explanatory.
	     std::string b_name = bones.at(i).getName();
	     std::string parent_name = FindAiNode(b_name)->mParent->mName.data;

	     Bone* p_bone = FindBone(parent_name);

	     bones.at(i).setParentBone(p_bone);

	     if(p_bone == nullptr)
	         std::cout<<"Parent Bone for "<<b_name<<" does not exist (is nullptr)"<<std::endl;
	 }
	 if(meshes.size() > 0)
	     meshes.at(0).skeleton->Init(bones,globalInverseTransform);
}

Mesh AnimatedModel::processMesh(aiMesh * mesh, const aiScene * scene) {
    vector<AnimatedVertex> vertices;
    vector<unsigned int> indices;
    vector<Texture> textures;

    for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
        AnimatedVertex vertex;
        // process vertex positions, normals and texture coordinates
        vec3 vector = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        vertex.Position = vector;
        vector = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        vertex.Normal = vector;
        if(mesh->mTextureCoords[0]) { // does the mesh contain texture coordinates?
        vec2 vec;
        vec.x = mesh->mTextureCoords[0][i].x; 
        vec.y = mesh->mTextureCoords[0][i].y;
        vertex.TexCoords = vec;
        }
        else
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);  
         vertices.push_back(vertex);
    }

    int boneArraysSize = mesh->mNumVertices*NUM_BONES_PER_VEREX;

    std::vector<int> boneIDs;
    boneIDs.resize(boneArraysSize);

    std::vector<float> boneWeights;
    boneWeights.resize(boneArraysSize);
    for(int i=0;i<mesh->mNumBones;i++) {

         aiBone* aiBone = mesh->mBones[i];

         for(int j=0;j<aiBone->mNumWeights;j++) {
              aiVertexWeight weight = aiBone->mWeights[j];
              unsigned int vertexStart = weight.mVertexId * NUM_BONES_PER_VEREX;
              for(int k=0;k<NUM_BONES_PER_VEREX;k++) {
                   if(boneWeights.at(vertexStart+k)==0) {
                   boneWeights.at(vertexStart+k) = weight.mWeight;
                   boneIDs.at(vertexStart+k) = i;
                   vertices.at(weight.mVertexId).BoneIDs[k] = i;
                   vertices.at(weight.mVertexId).Weights[k] = weight.mWeight;
                   break;
                   }
              }
         }
    }

    // process indices
    for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
      aiFace face = mesh->mFaces[i];
      for(unsigned int j = 0; j < face.mNumIndices; j++)
          indices.push_back(face.mIndices[j]);
  }  
    // process material
    if(mesh->mMaterialIndex >= 0) {
      aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
      vector<Texture> diffuseMaps = loadMaterialTextures(material, 
                                          aiTextureType_DIFFUSE, "texture_diffuse");
      textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
      vector<Texture> specularMaps = loadMaterialTextures(material, 
                                          aiTextureType_SPECULAR, "texture_specular");
      textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
  }  
    return AnimatedMesh(vertices, indices, textures);
}

void AnimatedModel::recursiveNodeProcess(aiNode* node) {
      nodes.push_back(node);

      for(int i = 0; i < node->mNumChildren; i++)
           recursiveNodeProcess(node->mChildren[i]);
}

void AnimatedModel::AnimNodeProcess() {
      if(scene->mNumAnimations == 0)
           return;

      for(int i = 0; i < scene->mAnimations[0]->mNumChannels; i++)
           nodes_anim.push_back(scene->mAnimations[0]->mChannels[i]);
      
      //We only get data from the first mAnimation because 
      //Assimp crushes all of the animation data into one
      //large sequence of data known as mAnimation.
      //Assimp does not support multiple mAnimations.
}

Bone* AnimatedModel::FindBone(std::string name) {
     for(int i = 0; i < bones.size(); i++)
     {
         if(bones.at(i).getName() == name)
             return &bones.at(i);
     }
     //This function simply scans our vector bones and checks if
     //any name matches the name we're looking for, if it doesn't
     //find any, we return nullptr.
     //Keep in mind, the bones vector is empty at the point of writing this,
     //but when this function is called it will already be filled up.
     return nullptr;
}

aiNode* AnimatedModel::FindAiNode(std::string name) {
     for(int i = 0; i < nodes.size(); i++)
     {
         if(nodes.at(i)->mName.data == name)
             return nodes.at(i);
     }
     //This function's purpose is identical, except that instead of Bones,
     //it's looking for an aiNode* inside our nodes vector.
     //This vector has already been filled by our recursiveNodeProcess() function.
     return nullptr;
}

aiNodeAnim* AnimatedModel::FindAiNodeAnim(std::string name) {
     for(int i = 0; i < nodes_anim.size(); i++)
     {
         if(nodes_anim.at(i)->mNodeName.data == name)
             return nodes_anim.at(i);
     }
     //This function finds the animation with the name we pass in, we called it
     //right after calling our recursiveNodeProcess() function, but this function
     //will only really come into play during the next tutorial, where we cover
     //the actual animation portion of skeletal animation.
     return nullptr;
}

int AnimatedModel::FindBoneIDByName(std::string name) {
     for(int i = 0; i < bones.size(); i++)
     {
         if(bones.at(i).getName() == name)
             return i;
     }
     //This function finds the position of a certain bone within our bones vector.
     //This position is equal to the bone's ID, which is vital to determining the
     //rigging of our model within the vertex shader.
     return -1;    //In case we don't find a bone ID, we return -1.
                   //Just to avoid any confusion later on as to whether or not the
                   //ID was found. (It serves the same purpose as returning nullptr).
}

