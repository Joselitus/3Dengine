#include "Model.h"

using namespace std;
using namespace glm;

Model::Model(const char * path) {
	this->loadModel((char*)path);
}

void Model::loadModel(string path) {
	Assimp::Importer import;
	scene = import.ReadFile(path, aiProcess_GenNormals | aiProcess_Triangulate | aiProcess_FlipUVs);

	if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        cout << "ERROR::ASSIMP::" << import.GetErrorString() << endl;
        return;
    }
    directory = path.substr(0, path.find_last_of('/'));

    this->processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode * node, const aiScene * scene) {
	// process all the node's meshes (if any)
    for(unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]]; 
        meshes.push_back(processMesh(mesh, scene));			
    }
    // then do the same for each of its children
    for(unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName) {
	vector<Texture> textures;
    for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;
        for(unsigned int j = 0; j < textures_loaded.size(); j++)
        {
            if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
            {
                textures.push_back(textures_loaded[j]);
                skip = true; 
                break;
            }
        }
        if(!skip)
        {   // if texture hasn't been loaded already, load it
            Texture texture;
            texture.id = TextureFromFile(str.C_Str(), directory);
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture); // add to loaded textures
        }
    }
    return textures;
}

Mesh Model::processMesh(aiMesh * mesh, const aiScene * scene) {
	vector<Vertex> vertices;
    vector<unsigned int> indices;
    vector<Texture> textures;

    for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
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

    return Mesh(vertices, indices, textures);
}

void Model::Draw(Shader * shader) {
    for(unsigned int i = 0; i < this->meshes.size(); i++)
        this->meshes[i].Draw(shader);
}  