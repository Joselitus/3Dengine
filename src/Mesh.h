#ifndef MESH
#define MESH

#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Shader.h"

struct Vertex {
	glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

};

struct Texture {
	unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
    protected:
        // mesh data
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture>      textures;

        //  render data
        unsigned int VAO, VBO, EBO;
        void setupMesh();

    public:
    	Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
        Mesh(std::vector<unsigned int> indices, std::vector<Texture> textures);
        virtual ~Mesh() = default;
        void Draw(Shader * shader);
};  

#endif