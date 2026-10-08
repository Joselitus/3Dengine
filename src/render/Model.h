#ifndef MODEL
#define MODEL

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <functional>
#include <memory>

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
        bool transparent = false; // some mesh is translucent (see Draw)

        virtual void loadModel(std::string path);
        void processNode(aiNode *node, const aiScene *scene);
        virtual Mesh processMesh(aiMesh *mesh, const aiScene *scene);
        std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, 
                                             std::string typeName);

    public:
        Model() {;} // Epico default constructor
        Model(const char * path);
        // Draws the opaque meshes, or (translucent = true) only the translucent
        // ones, which have to be drawn after everything opaque in the scene
        void Draw(Shader * shader, bool translucent = false);
        bool hasTransparent() const { return transparent; }
        // A copy of the model with its vertices moved (see Mesh::deformed): a crashed version of it
        std::shared_ptr<Model> deformed(const std::function<glm::vec3(const glm::vec3 &)> &move) const;
};

#endif
