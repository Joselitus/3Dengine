#ifndef LIGHT
#define LIGHT

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Shader.h"

class Light {
	private:
		Shader * shader;
		glm::vec3 color;
		glm::vec3 position;

	public:
		Light(float r, float g, float b, Shader * shader);
		void moveTo(float x, float y, float z);
		void update();

};

#endif