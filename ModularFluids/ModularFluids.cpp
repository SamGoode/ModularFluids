//#include "pch.h" // use stdafx.h in Visual Studio 2017 and earlier
#include <utility>
#include <limits.h>
#include "ModularFluids.h"

#include <fstream>
#include <string>
#include <iostream>

#include "glad.h"
#include <glfw/include/GLFW/glfw3.h>

#define RESOURCE_PATH "../../resources/";


#define MAX_PARTICLES 2048
//#define MAX_PARTICLES 262144

//#define MAX_PARTICLES_PER_CELL 16 // Only viable when using Mullet.M position based fluid technique
#define MAX_PARTICLES_PER_CELL 32

#define WORKGROUP_SIZE_X 1024

#define COMPUTE_CELLS_PER_WORKGROUP 16

#define PROJECTIONVIEW_UBO 0

#define FLUID_CONFIG_UBO 1
#define FLUID_DATA_SSBO 2


// DLL internal state variables:
static MF_GETPROCADDRESSPROC _glGetProcAddress = NULL;

static void* getProcAddress(const char* name) {
	return (void*)_glGetProcAddress(name);
}




class Shader {
protected:
	unsigned int gl_id = 0;

public:
	Shader() {}
	virtual ~Shader() { glDeleteProgram(gl_id); }

	virtual void init(const char* vertFileName, const char* fragFileName);
	void use() { glUseProgram(gl_id); }
	void bindUniform(const float& f, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform1f(uniformLocation, f); }
	void bindUniform(const int& i, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform1i(uniformLocation, i); }
	void bindUniform(const glm::vec2& v2, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform2fv(uniformLocation, 1, glm::value_ptr(v2)); }
	void bindUniform(const glm::vec3& v3, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniform3fv(uniformLocation, 1, glm::value_ptr(v3)); }
	void bindUniform(const glm::mat4& m4, const char* name) { unsigned int uniformLocation = glGetUniformLocation(gl_id, name); glUniformMatrix4fv(uniformLocation, 1, false, glm::value_ptr(m4)); }
	void bindUniformBuffer(GLuint bindingIndex, const char* name) { unsigned int uniformBlockIndex = glGetUniformBlockIndex(gl_id, name); glUniformBlockBinding(gl_id, uniformBlockIndex, bindingIndex); }

protected:
	unsigned int loadShaderFromFile(GLenum type, const char* fileName);
};

class ComputeShader : public Shader {
public:
	ComputeShader() {}

	virtual void init(const char* computeFileName, const char* empty = NULL) override;
};


void Shader::init(const char* vertFileName, const char* fragFileName) {
	assert(gl_id == 0 && "Shader already initialized");

	unsigned int vs = loadShaderFromFile(GL_VERTEX_SHADER, vertFileName);
	glCompileShader(vs);

	unsigned int fs = loadShaderFromFile(GL_FRAGMENT_SHADER, fragFileName);
	glCompileShader(fs);

	gl_id = glCreateProgram();
	glAttachShader(gl_id, vs);
	glAttachShader(gl_id, fs);
	glLinkProgram(gl_id);

	int success = GL_FALSE;
	glGetProgramiv(gl_id, GL_LINK_STATUS, &success);
	if (success == GL_FALSE) {
		int infoLogLength = 0;
		glGetProgramiv(gl_id, GL_INFO_LOG_LENGTH, &infoLogLength);
		char* infoLog = new char[infoLogLength + 1];

		glGetProgramInfoLog(gl_id, infoLogLength, 0, infoLog);
		printf("Error: Failed to link shader program!\nVertex Shader: %s\nFragment Shader: %s\n%s\n", vertFileName, fragFileName, infoLog);
		delete[] infoLog;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
}

unsigned int Shader::loadShaderFromFile(GLenum type, const char* fileName) {
	unsigned int shader = glCreateShader(type);

	std::string fullpath = RESOURCE_PATH;
	fullpath += fileName;

	std::ifstream fileStream;
	fileStream.open(fullpath, std::ios::in | std::ios::binary);

	std::string fileString;

	std::string line;
	while (std::getline(fileStream, line)) {
		if (line[0] == '#') {
			if (line.substr(1, 7) == "include") {
				size_t nameStart = line.find_first_of('"', 7) + 1;
				size_t nameEnd = line.find_first_of('"', nameStart);
				std::string name = RESOURCE_PATH;
				name += line.substr(nameStart, nameEnd - nameStart);

				std::ifstream includeFile;
				includeFile.open(name, std::ios::in | std::ios::binary);

				includeFile.seekg(0, includeFile.end);
				int fileLength = (int)includeFile.tellg();
				includeFile.seekg(0, includeFile.beg);

				char* buffer = new char[fileLength + 1];
				includeFile.read(buffer, fileLength);
				buffer[fileLength] = NULL;

				includeFile.close();

				fileString.append(buffer);
				fileString.append("\n");

				delete[] buffer;

				continue;
			}
		}
		fileString.append(line + "\n");
	}
	fileStream.close();

	//fileStream.read(test.data(), fileLength);
	//test.copy(fileText, sizeof(char) * (fileLength + 1));
	//fileString[fileLength] = NULL;

	const char* c_str = fileString.c_str();
	glShaderSource(shader, 1, &c_str, 0);
	//glShaderSource(shader, 1, (const char**)&fileText, 0);

	//delete[] fileText;

	return shader;
}





void ComputeShader::init(const char* computeFileName, const char* empty) {
	assert(gl_id == 0 && "Shader already initialized");

	unsigned int cs = loadShaderFromFile(GL_COMPUTE_SHADER, computeFileName);
	glCompileShader(cs);

	gl_id = glCreateProgram();
	glAttachShader(gl_id, cs);
	glLinkProgram(gl_id);

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










struct uboData {
	glm::vec4 boundsMin;
	glm::vec4 boundsMax;

	glm::vec4 gravity;
	float smoothingRadius;
	float restDensity;
	float particleMass;

	float stiffness;
	float nearStiffness;

	float timeStep;
	unsigned int particleCount;
};

//struct ssboData {
//	vec4 positions[MAX_PARTICLES];
//	vec4 previousPositions[MAX_PARTICLES];
//	vec4 velocities[MAX_PARTICLES];
//
//	float lambdas[MAX_PARTICLES];
//	float densities[MAX_PARTICLES];
//	float nearDensities[MAX_PARTICLES];
//
//	unsigned int usedCells;
//	unsigned int hashes[MAX_PARTICLES];
//	unsigned int hashTable[MAX_PARTICLES];// clear this one
//	unsigned int cellEntries[MAX_PARTICLES];// clear this one
//	unsigned int cells[MAX_PARTICLES * MAX_PARTICLES_PER_CELL];// clear this one
//};

class UBO {
private:
	unsigned int ubo_id = 0;

public:
	UBO() {}
	~UBO() { glDeleteBuffers(1, &ubo_id); }

	void init(GLsizeiptr size) {
		assert(ubo_id == 0 && "Shader storage buffer already initialized");

		glGenBuffers(1, &ubo_id);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_id);
		glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void subData(GLintptr offset, GLsizeiptr size, const void* data) {
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_id);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void bindBufferBase(GLuint bindingIndex) { glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, ubo_id); }
};

class SSBO {
private:
	unsigned int ssbo_id = 0;

public:
	SSBO() {}
	~SSBO() { glDeleteBuffers(1, &ssbo_id); }

	void init(GLsizeiptr size) {
		assert(ssbo_id == 0 && "Shader storage buffer already initialized");

		glGenBuffers(1, &ssbo_id);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void subData(GLintptr offset, GLsizeiptr size, const void* data) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	// Sets all internal SSBO data to 0x00000000.
	void clearBufferData() { unsigned int zero = 0x00000000; glClearNamedBufferData(ssbo_id, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero); }
	void clearNamedSubData(GLenum internalFormat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
		glClearNamedBufferSubData(ssbo_id, internalFormat, offset, size, format, type, data);
	}
	void getSubData(GLintptr offset, GLsizeiptr size, void* data) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void bindBufferBase(GLuint bindingIndex) { glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingIndex, ssbo_id); }
	void bindAsIndirect() { glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, ssbo_id); }
};

class SPH_Compute : public ISPH_Compute {
private:
	glm::vec3 position = glm::vec3(0);
	glm::vec3 bounds = glm::vec3(0);

	const float fixedTimeStep = 0.01f;

	glm::vec3 gravity = glm::vec3(0.f);
	float particleRadius;
	float smoothingRadius; // density kernel radius
	float restDensity;
	float particleMass;

	// Clavet.S parameters
	float stiffness;
	float nearStiffness;

	unsigned int particleCount = 0;

	UBO configUBO;
	SSBO particleSSBO;
	SSBO indirectCmdsSSBO;

	ComputeShader particleComputeShader;
	ComputeShader computeHashTableShader;
	ComputeShader computeDensityShader;
	ComputeShader computePressureShader;

	// Buffer for particle position data.
	glm::vec4 positionBuffer[1024];

public:
	SPH_Compute() {}
	~SPH_Compute() {}

	virtual void init(glm::vec3 _position, glm::vec3 _bounds, glm::vec3 _gravity, float _particleRadius = 0.4f,
		float _restDensity = 1000.f, float _stiffness = 20.f, float _nearStiffness = 80.f) override;

	virtual void stepSim() override;

	// Stores simulation parameters in a buffer and then sends buffer data to GPU.
	virtual void syncUBO() override;
	virtual void resetHashDataSSBO() override;

	virtual unsigned int getParticleCount() override { return particleCount; }
	virtual void spawnRandomParticles(unsigned int spawnCount) override;
	virtual void clearParticles() override { particleCount = 0; }

	virtual void bindConfigUBO(GLuint bindingIndex) override { configUBO.bindBufferBase(bindingIndex); }
	virtual void bindParticleSSBO(GLuint bindingIndex) override { particleSSBO.bindBufferBase(bindingIndex); }
	virtual void bindIndirectCmdsSSBO(GLuint bindingIndex) override { indirectCmdsSSBO.bindBufferBase(bindingIndex); }
	virtual void useIndirectCmdsSSBO() override { indirectCmdsSSBO.bindAsIndirect(); }
	virtual void getIndirectCmdsData(void* data) { indirectCmdsSSBO.getSubData(0, sizeof(unsigned int) * 3, data); }
};

void SPH_Compute::init(glm::vec3 _position, glm::vec3 _bounds, glm::vec3 _gravity, float _particleRadius,
	float _restDensity, float _stiffness, float _nearStiffness) {

	position = _position;
	bounds = _bounds;
	gravity = _gravity;

	particleRadius = _particleRadius;
	smoothingRadius = _particleRadius / 4.f; // (recommended on compsci stack exchange)
	restDensity = _restDensity;

	// particle mass calculation based on radius and rest density
	float particleVolume = (_particleRadius * _particleRadius * _particleRadius * 4.f * glm::pi<float>()) / 3.f; // metres^3
	constexpr unsigned int estimatedNeighbours = 20;
	particleMass = (particleVolume * restDensity) / (float)estimatedNeighbours; // kgs

	// Clavet.S parameters
	stiffness = _stiffness;
	nearStiffness = _nearStiffness;

	// UBO for simulation parameters
	configUBO.init(sizeof(uboData));
	syncUBO();

	// SSBO for particle data
	GLsizeiptr sizePerParticle = sizeof(glm::vec4) * 3
		+ sizeof(float) * 6
		+ sizeof(float) * MAX_PARTICLES_PER_CELL;
	particleSSBO.init(MAX_PARTICLES * sizePerParticle + sizeof(float));
	particleSSBO.clearBufferData();

	// SSBO for indirectDispatchCommands
	indirectCmdsSSBO.init(3 * sizeof(unsigned int));
	indirectCmdsSSBO.clearBufferData();

	// Compute Shaders
	particleComputeShader.init("shaders/particleCompute.glsl");
	computeHashTableShader.init("shaders/buildHashTable.glsl");
	computeDensityShader.init("shaders/computeDensity.glsl");
	computePressureShader.init("shaders/computePressure.glsl");
}

void SPH_Compute::stepSim() {
	resetHashDataSSBO();
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	indirectCmdsSSBO.bindBufferBase(3);

	particleComputeShader.use();
	glDispatchCompute((particleCount / WORKGROUP_SIZE_X) + ((particleCount % WORKGROUP_SIZE_X) != 0), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	unsigned int usedCells;
	particleSSBO.getSubData(15 * MAX_PARTICLES * sizeof(float), sizeof(float), &usedCells);
	std::cout << "used cells: " << usedCells << std::endl;

	computeHashTableShader.use();
	glDispatchCompute((particleCount / WORKGROUP_SIZE_X) + ((particleCount % WORKGROUP_SIZE_X) != 0), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	indirectCmdsSSBO.bindAsIndirect();
	glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

	unsigned int cmd[3];
	indirectCmdsSSBO.getSubData(0, sizeof(unsigned int) * 3, cmd);
	std::cout << "x: " << cmd[0] << ", y: " << cmd[1] << ", z: " << cmd[2] << std::endl;

	//for (unsigned int iteration = 0; iteration < solverIterations; iteration++) {
	int time = (int)std::time(0);

	computeDensityShader.use();
	//computeDensityShader.bindUniform(time, "time");
	glDispatchComputeIndirect(0);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	computePressureShader.use();
	computePressureShader.bindUniform(time, "time");
	glDispatchComputeIndirect(0);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	//}
}

// Stores simulation parameters in a buffer and then sends buffer data to GPU.
void SPH_Compute::syncUBO() {
	uboData tempBuffer = {
		glm::vec4(position, 0),
		glm::vec4(position + bounds, 0),

		glm::vec4(gravity, 0),
		smoothingRadius,
		restDensity,
		particleMass,

		stiffness,
		nearStiffness,

		fixedTimeStep,
		particleCount
	};

	configUBO.subData(0, sizeof(uboData), &tempBuffer);
}

void SPH_Compute::resetHashDataSSBO() {
	const unsigned int usedCells = 0;
	const unsigned int defaultHash = 0xFFFFFFFF;
	particleSSBO.subData(15 * MAX_PARTICLES * sizeof(float), sizeof(float), &usedCells);
	//particleSSBO->clearNamedSubData(GL_R32UI, 15 * MAX_PARTICLES * sizeof(float), sizeof(float), GL_RED_INTEGER, GL_UNSIGNED_INT, &usedCells);
	particleSSBO.clearNamedSubData(GL_R32UI, ((16 * MAX_PARTICLES) + 1) * sizeof(float), MAX_PARTICLES * sizeof(float), GL_RED_INTEGER, GL_UNSIGNED_INT, &defaultHash);
	particleSSBO.clearNamedSubData(GL_R32UI, ((17 * MAX_PARTICLES) + 1) * sizeof(float), MAX_PARTICLES * sizeof(float), GL_RED_INTEGER, GL_UNSIGNED_INT, &usedCells);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
}
//struct ssboData {
//	vec4 positions[MAX_PARTICLES];
//	vec4 previousPositions[MAX_PARTICLES];
//	vec4 velocities[MAX_PARTICLES];
//
//	float lambdas[MAX_PARTICLES];
//	float densities[MAX_PARTICLES];
//	float nearDensities[MAX_PARTICLES];
//
//	unsigned int usedCells;
//	unsigned int hashes[MAX_PARTICLES];
//	unsigned int hashTable[MAX_PARTICLES];// clear this one
//	unsigned int cellEntries[MAX_PARTICLES];// clear this one
//	unsigned int cells[MAX_PARTICLES * MAX_PARTICLES_PER_CELL];// clear this one
//};


// Spawns particles randomly within simulation bounds in batches of 1024.
void SPH_Compute::spawnRandomParticles(unsigned int spawnCount) {
	assert(particleCount + spawnCount <= MAX_PARTICLES);

	unsigned int i = 0;
	while (i < spawnCount) {
		unsigned int batchCount = 0;
		while (batchCount < 1024 && i < spawnCount) {
			glm::vec3 randomPosition = glm::linearRand(position, position + bounds);
			positionBuffer[batchCount] = glm::vec4(randomPosition, 0);

			batchCount++;
			i++;
		}

		// Fill position and previous position memory chunk.
		particleSSBO.subData((particleCount) * sizeof(glm::vec4), batchCount * sizeof(glm::vec4), positionBuffer);
		particleSSBO.subData((MAX_PARTICLES + particleCount) * sizeof(glm::vec4), batchCount * sizeof(glm::vec4), positionBuffer);
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

		particleCount += batchCount;
	}
		
	syncUBO();
}




// Might use later
//// Mullen.M parameters
//float epsilon = 0.f;
//float k = 0.f;
//int N = 0;
//float densityDeltaQ = 0.f;

//// Precomputed values
//sqrSmoothingRadius = smoothingRadius * smoothingRadius;
//normFactor_P6 = 315.f / (64.f * glm::pi<float>() * glm::pow<float>(smoothingRadius, 9));
//normFactor_S = 45.f / (glm::pi<float>() * glm::pow<float>(smoothingRadius, 6));


namespace ModularFluids
{
	void LoadLib(MF_GETPROCADDRESSPROC funcPtr) {
		_glGetProcAddress = funcPtr;

		gladLoadGLLoader(&getProcAddress);
		std::cout << "ModularFluids glDispatchComputeIndirect: " << glad_glDispatchComputeIndirect << std::endl;
	}

	ISPH_Compute* Create() {
		return new SPH_Compute();
	}

	void Destroy(ISPH_Compute* instance) {
		delete instance;
	}

	void Init(ISPH_Compute* instance,
		glm::vec3 position, glm::vec3 bounds, glm::vec3 gravity,
		float particleRadius, float restDensity, float stiffness, float nearStiffness) {

		instance->init(position, bounds, gravity, particleRadius, restDensity, stiffness, nearStiffness);
	}

	void StepSim(ISPH_Compute* instance) {
		instance->stepSim();
	}

	unsigned int GetParticleCount(ISPH_Compute* instance) {
		return instance->getParticleCount();
	}

	void SpawnParticles(ISPH_Compute* instance, unsigned int particleCount) {
		instance->spawnRandomParticles(particleCount);
	}

	void ClearParticles(ISPH_Compute* instance) {
		instance->clearParticles();
	}

	void SyncUBO(ISPH_Compute* instance) {
		instance->syncUBO();
	}

	void ResetHashData(ISPH_Compute* instance) {
		instance->resetHashDataSSBO();
	}

	void BindConfigUBO(ISPH_Compute* instance, unsigned int bindingIndex) {
		instance->bindConfigUBO(bindingIndex);
	}

	void BindParticleSSBO(ISPH_Compute* instance, unsigned int bindingIndex) {
		instance->bindParticleSSBO(bindingIndex);
	}
}
