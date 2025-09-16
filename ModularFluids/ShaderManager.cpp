#include "ShaderManager.h"

#include "glad.h"
#include <glfw/include/GLFW/glfw3.h>

#include "ResourceManager.h"

#include "resource.h"


Shader::~Shader() { glDeleteProgram(gl_id); }

void Shader::init(const char* vertFileName, const char* fragFileName) {} // FIX LATER
void Shader::use() { glUseProgram(gl_id); }

void Shader::bindUniform(const float& f, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform1f(uniformLocation, f); }
void Shader::bindUniform(const int& i, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform1i(uniformLocation, i); }
void Shader::bindUniform(const glm::vec2& v2, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform2fv(uniformLocation, 1, glm::value_ptr(v2)); }
void Shader::bindUniform(const glm::vec3& v3, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform3fv(uniformLocation, 1, glm::value_ptr(v3)); }
void Shader::bindUniform(const glm::mat4& m4, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniformMatrix4fv(uniformLocation, 1, false, glm::value_ptr(m4)); }

void Shader::bindUniformBuffer(unsigned int bindingIndex, const char* name) { unsigned int uniformBlockIndex = glGetUniformBlockIndex(gl_id, name); glUniformBlockBinding(gl_id, uniformBlockIndex, bindingIndex); }


unsigned int Shader::loadShaderFromText(unsigned int type, const char* srcCodeText) {
	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, &srcCodeText, 0);
	return shader;
}


void ComputeShader::init(const char* srcCodeTxt, const char* empty) {
	assert(gl_id == 0 && "Shader already initialized");

	unsigned int cs = loadShaderFromText(GL_COMPUTE_SHADER, srcCodeTxt);
	glCompileShader(cs);

	gl_id = glCreateProgram();
	glAttachShader(gl_id, cs);
	glLinkProgram(gl_id);

	glValidateProgram(gl_id);

	int success = GL_FALSE;
	glGetProgramiv(gl_id, GL_LINK_STATUS, &success);
	if (success == GL_FALSE) {
		int infoLogLength = 0;
		glGetProgramiv(gl_id, GL_INFO_LOG_LENGTH, &infoLogLength);
		char* infoLog = new char[infoLogLength + 1];

		glGetProgramInfoLog(gl_id, infoLogLength, 0, infoLog);
		printf("Error: Failed to link shader program!\n%s\n", infoLog);
		delete[] infoLog;
	}

	glDeleteShader(cs);
}



// ShaderManager internal variables
static std::string version = "#version 460\n";
static std::string setMaxParticles = "#define MAX_PARTICLES 32768\n";

static void load_shader(ComputeShader& compute, int shaderResource_id) {
	std::string configStr = std::string(ResourceManager::GetResource(IDR_CONFIG)->toString());
	std::string compStr = std::string(ResourceManager::GetResource(shaderResource_id)->toString());
	
	//std::string out = version + configStr + '\n' + compStr;
	std::string out = version + setMaxParticles + configStr + '\n' + compStr;
	compute.init(out.c_str());
}


namespace ShaderManager {

	void LoadShader_Particle(ComputeShader& compute) {
		load_shader(compute, IDR_COMP_PARTICLE);
	}

	void LoadShader_HashTable(ComputeShader& compute) {
		load_shader(compute, IDR_COMP_HASHTABLE);
	}

	void LoadShader_Density(ComputeShader& compute) {
		load_shader(compute, IDR_COMP_DENSITY);
	}

	void LoadShader_Pressure(ComputeShader& compute) {
		load_shader(compute, IDR_COMP_PRESSURE);
	}
}

