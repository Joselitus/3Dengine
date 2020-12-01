#ifndef MODEL
#define MODEL

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"


class Model 
{
    public:
        const aiScene * scene;
        std::vector<aiNode*> nodes;
        // model data
        std::vector<Mesh> meshes;
        std::vector<Texture> textures_loaded; 
        std::string directory;

        virtual void loadModel(std::string path);
        void processNode(aiNode *node, const aiScene *scene);
        virtual Mesh processMesh(aiMesh *mesh, const aiScene *scene);
        std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, 
                                             std::string typeName);

    public:
        Model(const char * path);
        void Draw(Shader * shader); 
};

#endif