#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "myopengl.h"
#include "Model.h"
#include "Skeleton.h"
#include "AnimatedModel.h"
#include "AnimatedMesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Light.h"
#include "GameObject.h"

using namespace std;
using namespace glm;

#define FULLSCREEN 0
#define HEIGHT 600
#define WIDTH 700

// Global variables

const char * WINDOWNAME = "test bimbow";  

//Vertex shader
const char * vertexShaderFile = "shader.vert";

//Fragment shader
const char * fragmentShaderFile = "shader.frag";


GLFWwindow * initializeGLFW(const char * windowname) {
	// Initialise GLFW
	glewExperimental = true; // Needed for core profile
	if( !glfwInit() )
	{
	    fprintf( stderr, "Failed to initialize GLFW\n" );
	    return NULL;
	}

	glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

	// Open a window and create its OpenGL context
	GLFWwindow* window; // (In the accompanying source code, this variable is global for simplicity)
	if ( FULLSCREEN ) {
		const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	    int window_width = mode->width;
	    int window_height = mode->height;
	    window = glfwCreateWindow( window_width, window_height, windowname, glfwGetPrimaryMonitor(), NULL);

	}
	else
		window = glfwCreateWindow( WIDTH, HEIGHT, windowname, NULL, NULL);

	if( window == NULL ){
	    fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
	    glfwTerminate();
	    return NULL;
	}
	glfwMakeContextCurrent(window); // Initialize GLEW
	glewExperimental=true; // Needed in core profile
	if (glewInit() != GLEW_OK) {
	    fprintf(stderr, "Failed to initialize GLEW\n");
	    return NULL;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	// Disable cursor
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	return window;
}

void processInput(GLFWwindow * window, Camera * camera) {

	double xpos, ypos;

	glfwGetCursorPos(window, &xpos, &ypos);

	if (xpos != camera->getPhi() || ypos != camera->getTheta())
		camera->rotate(xpos, ypos);

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera->move(0.0f, 0.0f, 1.0f);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera->move(0.0f, 0.0f, -1.0f);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera->move(-1.0f, 0.0f, 0.0f);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera->move(1.0f, 0.0f, 0.0f);
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		camera->move(0.0f, -1.0f, 0.0f);
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		camera->move(0.0f, 1.0f, 0.0f);

	if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        //glfwSetWindowShouldClose(window, true);
}


int main() {

	// Creation of window and it's context
	GLFWwindow * window;
	if ( !(window = initializeGLFW(WINDOWNAME)) )
		return -1;

	// Enable depth test
	glEnable(GL_DEPTH_TEST);  

	// Initialization of shaders
	Shader shader(vertexShaderFile, fragmentShaderFile);

	// Creation of camera
	Camera camera(window, &shader);
	camera.reposition(0.0, 0.0, 3.0);

	// Creation of light
	Light light(1.0f, 1.0f, 1.0f, &shader);
	light.moveTo(0.0, 100.0, -10.0);

	Model model("../assets/ping/ping.obj");
	GameObject ping(&model);

	vec3 pengposition = vec3(0.0, 0.0, 0.0);
	vec3 rotation = vec3(0.0, 0.0, 0.0);
	mat4 rotm;


	// Main loop
	while ( !glfwWindowShouldClose(window) ) {
	    // Clear the screen. It can cause flickering, so it's there nonetheless.
	    processInput(window, &camera);
	    glClearColor(0.3f, 0.3f, 1.0f, 1.0f);
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		// Magic
		rotation += vec3(0.001, 0.007, 0.002);
		rotm = glm::rotate(mat4(1.0), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		rotm = glm::rotate(rotm, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		rotm = glm::rotate(rotm, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
		pengposition += vec3(0.001, 0.004, 0.002);
		shader.setMatrix4("pengrotation", value_ptr(rotm));
		ping.setPosition(pengposition.x, pengposition.y, pengposition.z);
	    // Draw

	    ping.Draw(&shader);
	    

	    // Swap buffers
	    glfwSwapBuffers(window);
	    glfwPollEvents();

	}

	glfwTerminate();
	return 0;
}