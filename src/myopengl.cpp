#include "myopengl.h"

using namespace std;


char * fileToString(const char * file) {
	string line,text;
    ifstream in(file);
    while( getline(in, line) ) text += line + "\n";
    char * data = (char*)malloc(sizeof(char)*(text.length()+1));
    strncpy((char*)data, text.c_str(), text.length());
    data[text.length()] = '\0';
    return data;
}

unsigned int TextureFromFile(const char * name, string directory) {
	char * path = (char*)malloc(120);
	strcpy(path, directory.c_str());
	strcat(path, "/");
	strcat(path, name);
	unsigned int textureid = loadTexture((const char *)path);
	free(path);
	return textureid;
}

aiMatrix4x4 GLMMat4ToAi(glm::mat4 mat) {
     return aiMatrix4x4(mat[0][0],mat[0][1],mat[0][2],mat[0][3],
                        mat[1][0],mat[1][1],mat[1][2],mat[1][3],
                        mat[2][0],mat[2][1],mat[2][2],mat[2][3],
                        mat[3][0],mat[3][1],mat[3][2],mat[3][3]);
}

// Assimp matrices are row-major (translation in a4,b4,c4), glm is
// column-major (translation in mat[3]).
glm::mat4 AiToGLMMat4(aiMatrix4x4& in_mat) {
      glm::mat4 tmp;
      for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                  tmp[col][row] = in_mat[row][col];
      return tmp;
}

unsigned int initializeShaders(const char * vertexShaderFile, const char * fragmentShaderFile) {
	char * vertexShaderSource = fileToString(vertexShaderFile);
	unsigned int vertexShader;
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	free(vertexShaderSource);

	//Check vertex errors
	int  success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if ( !success ) {
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    	cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    	exit( EXIT_FAILURE );
	}

	char * fragmentShaderSource = fileToString(fragmentShaderFile);
	unsigned int fragmentShader;
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	free(fragmentShaderSource);

	//Check fragment errors
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if ( !success ) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    	cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    	exit( EXIT_FAILURE );
	}

	//Final shader
	unsigned int shaderProgram;
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	//Check linking errors
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if(!success) {
    	glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    	cout << "ERROR::SHADER::LINKING::COMPILATION_FAILED\n" << infoLog << endl;
    	exit( EXIT_FAILURE );
	}

	glUseProgram(shaderProgram);

	// We wont need these any more
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);  

	// Return the program
	return shaderProgram;
}

void initializeVertexBuffers(float * vertices, int lenght, int * atribute_count, int atributes) {
	unsigned int VBO;
	int atsum, curratsum;
	atsum = curratsum = 0;
	glGenBuffers(1, &VBO);  
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, lenght, vertices, GL_STATIC_DRAW);
	for (int i = 0; i < atributes; i++) atsum += atribute_count[i];
	for (int i = 0; i < atributes; i++) {
		glVertexAttribPointer(i, atribute_count[i], GL_FLOAT, GL_FALSE, atsum*sizeof(float), (void*)(curratsum*sizeof(float)));
		glEnableVertexAttribArray(i); 
		curratsum += atribute_count[i];
	}
}

unsigned int initializeIndexBuffers(unsigned int * indices, int lenght) {
	unsigned int EBO;
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, lenght, indices, GL_STATIC_DRAW);
	return EBO;
}

unsigned int bindNewVAO() {
	unsigned int VAO;
	glGenVertexArrays(1, &VAO);  
	glBindVertexArray(VAO);
	return VAO;
}

unsigned int newVAO(float * vertices, int lenght, int * atribute_sizes, int atributes) {
	unsigned int VAO = bindNewVAO();
	initializeVertexBuffers(vertices, lenght, atribute_sizes, atributes);
	return VAO;
}

unsigned int newIndexVAO(float * vertices, int lenght, int * atribute_sizes, int atributes, unsigned int * indices, int lenght_indices) {
	unsigned int VAO = bindNewVAO();
	initializeVertexBuffers(vertices, lenght, atribute_sizes, atributes);
	unsigned int EBO = initializeIndexBuffers(indices, lenght_indices);
	return VAO;
}

unsigned int loadTexture(const char * path) {
	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Texture settings
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load image
	int width, height, nrChannels;
	unsigned char * data; 
	if (TEXTURES_INVERTED) stbi_set_flip_vertically_on_load(true);  
	if ( data = stbi_load(path, &width, &height, &nrChannels, 0)) {
		GLenum format = nrChannels == 4 ? GL_RGBA : nrChannels == 1 ? GL_RED : GL_RGB;
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    	glGenerateMipmap(GL_TEXTURE_2D);
    	stbi_image_free(data);
    	return texture;
	}

	else {
		cout << "Failed to load texture" << endl;
		return 0;
	}

}