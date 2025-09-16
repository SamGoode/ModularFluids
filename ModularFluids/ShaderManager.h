#pragma once

#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>
#include <glm/glm/fwd.hpp>


class Shader {
protected:
	unsigned int gl_id = 0;

public:
	Shader() {}
	virtual ~Shader();

	virtual void init(const char* vertSrcTxt, const char* fragSrcTxt);
	void use();
	void bindUniform(const float& f, const char* name);
	void bindUniform(const int& i, const char* name);
	void bindUniform(const glm::vec2& v2, const char* name);
	void bindUniform(const glm::vec3& v3, const char* name);
	void bindUniform(const glm::mat4& m4, const char* name);
	void bindUniformBuffer(unsigned int bindingIndex, const char* name);

protected:
	unsigned int loadShaderFromText(unsigned int type, const char* srcCodeTxt);
};


class ComputeShader : public Shader {
public:
	ComputeShader() {}

	virtual void init(const char* srcCodeTxt, const char* empty = NULL) override;
};


// Handles compiling shaders with 'embedded' runtime data
namespace ShaderManager {
	//void LoadShaders();

	void LoadShader_Particle(ComputeShader& compute);
	void LoadShader_HashTable(ComputeShader& compute);
	void LoadShader_Density(ComputeShader& compute);
	void LoadShader_Pressure(ComputeShader& compute);
}