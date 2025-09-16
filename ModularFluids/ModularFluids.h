#pragma once

#ifdef MODULARFLUIDS_EXPORTS
#define MODULARFLUIDS_API __declspec(dllexport)
#else
#define MODULARFLUIDS_API __declspec(dllimport)
#endif


#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>
#include <glm/glm/fwd.hpp>

// units in metres and kgs
// particle spherical volume would be pi*4*r^3/3
// 0.004*pi/3 metres cubed
// water density is 1000 kg per cube metre
// with water density, 0.1 m radius sphere would have mass PI*4/3 kgs.
// approx. 4.18879 kilograms
// divide this evenly among estimated number of neighbouring particles (30-40) n = 30 for now.


class ISPH_Compute {
public:
	virtual ~ISPH_Compute() = 0 {}

	virtual void init(glm::vec3 _position, glm::vec3 _bounds, glm::vec3 _gravity, float _particleRadius = 0.4f,
		float _restDensity = 1000.f, float _stiffness = 20.f, float _nearStiffness = 80.f) = 0;

	// deltaTime is in seconds.
	virtual void update(float deltaTime) = 0;
	virtual void stepSim() = 0;

	// Stores simulation parameters in a buffer and then sends buffer data to GPU.
	virtual void syncUBO() = 0;
	virtual void resetHashDataSSBO() = 0;

	// Spawns particles randomly within simulation bounds in batches of 1024.
	virtual void spawnRandomParticles(unsigned int spawnCount) = 0;
	virtual unsigned int getParticleCount() = 0;
	virtual void clearParticles() = 0;

	virtual void bindConfigUBO(unsigned int bindingIndex) = 0;
	virtual void bindParticleSSBO(unsigned int bindingIndex) = 0;
	virtual void bindIndirectCmdsSSBO(unsigned int bindingIndex) = 0;
	virtual void useIndirectCmdsSSBO() = 0;
	virtual void getIndirectCmdsData(void* data) = 0;
};


typedef void(*procAddress)(void);
typedef procAddress (*MF_GETPROCADDRESSPROC)(const char* procname);


namespace ModularFluids {
	extern "C" MODULARFLUIDS_API void LoadLib(MF_GETPROCADDRESSPROC);

	extern "C" MODULARFLUIDS_API ISPH_Compute* Create();
	extern "C" MODULARFLUIDS_API void Destroy(ISPH_Compute* instance);

	extern "C" MODULARFLUIDS_API void Init(ISPH_Compute* instance,
		glm::vec3 position, glm::vec3 bounds, glm::vec3 gravity,
		float particleRadius = 0.4f, float restDensity = 1000.f, float stiffness = 20.f, float nearStiffness = 80.f);

	extern "C" MODULARFLUIDS_API void Update(ISPH_Compute* instance);
	extern "C" MODULARFLUIDS_API void StepSim(ISPH_Compute* instance);
}