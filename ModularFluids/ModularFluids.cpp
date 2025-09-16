//#include "pch.h" // use stdafx.h in Visual Studio 2017 and earlier
#include <utility>
#include <limits.h>
#include "ModularFluids.h"

// Must come before windows.h includes
#include "glad.h"
#include <glfw/include/GLFW/glfw3.h>

#include <fstream>
#include <string>	
#include <iostream>

#include "resource.h"
#include "ResourceManager.h"
#include "ShaderManager.h"


#define MAX_PARTICLES 32768

//#define MAX_PARTICLES_PER_CELL 16 // Only viable when using Mullet.M position based fluid technique
#define MAX_PARTICLES_PER_CELL 32

#define WORKGROUP_SIZE_X 1024

#define COMPUTE_CELLS_PER_WORKGROUP 16

#define FLUID_CONFIG_UBO 1
#define FLUID_DATA_SSBO 2


// DLL internal state variables:
static const unsigned int zero = 0;
static const unsigned int uintMax = 0xFFFFFFFF;


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
	const unsigned int solverIterations = 2;
	const unsigned int maxTicksPerUpdate = 8;
	const float fixedTimeStep = 0.01f;
	float accumulatedTime = 0.f;

	glm::vec3 position = glm::vec3(0);
	glm::vec3 bounds = glm::vec3(0);

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

	virtual void update(float deltaTime) override;
	virtual void stepSim() override;

	// Stores simulation parameters in a buffer and then sends buffer data to GPU.
	virtual void syncUBO() override;
	virtual void resetHashDataSSBO() override;

	virtual void spawnRandomParticles(unsigned int spawnCount) override;
	virtual unsigned int getParticleCount() override { return particleCount; }
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
	ShaderManager::LoadShader_Particle(particleComputeShader);
	ShaderManager::LoadShader_HashTable(computeHashTableShader);
	ShaderManager::LoadShader_Density(computeDensityShader);
	ShaderManager::LoadShader_Pressure(computePressureShader);
}

void SPH_Compute::update(float deltaTime) {
	accumulatedTime += deltaTime;

	syncUBO();

	for (unsigned int step = 0; step < maxTicksPerUpdate && accumulatedTime > fixedTimeStep; step++) {
		accumulatedTime -= fixedTimeStep;

		stepSim();
	}
}

void SPH_Compute::stepSim() {
	resetHashDataSSBO();
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

	indirectCmdsSSBO.bindBufferBase(3);

	particleComputeShader.use();
	glDispatchCompute((particleCount / WORKGROUP_SIZE_X) + ((particleCount % WORKGROUP_SIZE_X) != 0), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	computeHashTableShader.use();
	glDispatchCompute((particleCount / WORKGROUP_SIZE_X) + ((particleCount % WORKGROUP_SIZE_X) != 0), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	indirectCmdsSSBO.bindAsIndirect();
	glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

	int time = (int)std::time(0);
	for (unsigned int iteration = 0; iteration < solverIterations; iteration++) {
		computeDensityShader.use();
		glDispatchComputeIndirect(0);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		computePressureShader.use();
		computePressureShader.bindUniform(time, "time");
		glDispatchComputeIndirect(0);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}
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
	//particleSSBO.subData(15 * MAX_PARTICLES * sizeof(float), sizeof(float), &zero);//&usedCells);
	particleSSBO.clearNamedSubData(GL_R32UI, 15 * MAX_PARTICLES * sizeof(float), sizeof(float), GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
	particleSSBO.clearNamedSubData(GL_R32UI, ((16 * MAX_PARTICLES) + 1) * sizeof(float), MAX_PARTICLES * sizeof(float), GL_RED_INTEGER, GL_UNSIGNED_INT, &uintMax);
	particleSSBO.clearNamedSubData(GL_R32UI, ((17 * MAX_PARTICLES) + 1) * sizeof(float), MAX_PARTICLES * sizeof(float), GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
}


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

		particleCount += batchCount;
	}
		
	syncUBO();
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
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
		// BeeMovie script
		//std::cout << ResourceManager::GetResource(IDR_BEEMOVIE)->toString() << std::endl;


		gladLoadGLLoader((GLADloadproc)funcPtr);
		std::cout << "ModularFluids glDispatchComputeIndirect: " << glad_glDispatchComputeIndirect << std::endl;
	}

	ISPH_Compute* Create() { return new SPH_Compute(); }
	void Destroy(ISPH_Compute* instance) { delete instance; }

	void Init(ISPH_Compute* instance,
		glm::vec3 position, glm::vec3 bounds, glm::vec3 gravity,
		float particleRadius, float restDensity, float stiffness, float nearStiffness) {

		instance->init(position, bounds, gravity, particleRadius, restDensity, stiffness, nearStiffness);
	}

	void Update(ISPH_Compute* instance, float deltaTime) { instance->update(deltaTime); }
	void StepSim(ISPH_Compute* instance) { instance->stepSim(); }
}
