#ifndef MODEL
#define MODEL

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"


// Static model loaded with Assimp (OBJ, FBX...): one Mesh per aiMesh, with
// the diffuse/specular textures of its material (cached by path). Node
// transforms are ignored, the vertices are used as stored in the file.
// `scene` is only valid inside loadModel (the importer is a local there).
class Model 
{
    public:
        const aiScene * scene = nullptr; // only valid during loadModel
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
        Model() {;} // Epico default constructor
        Model(const char * path);
        void Draw(Shader * shader); 
};

#endif
