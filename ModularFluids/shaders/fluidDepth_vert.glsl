#version 430 core

#include "common.h"


layout(binding = PROJECTIONVIEW_UBO, std140) uniform PVMatrices {
	mat4 View;
	mat4 Projection;
	mat4 ViewInverse;
	mat4 ProjectionInverse;
	vec4 CameraPos;
};

layout(binding = FLUID_CONFIG_UBO, std140) uniform FluidConfig {
	vec4 boundsMin;
	vec4 boundsMax;

	vec4 gravity;
	float smoothingRadius;
	float restDensity;
	float particleMass;

	float stiffness;
	float nearStiffness;
	
	float timeStep;
	uint particleCount;
} config;

layout(binding = FLUID_DATA_SSBO, std430) readonly restrict buffer FluidData {
	vec4 positions[MAX_PARTICLES];
	vec4 previousPositions[MAX_PARTICLES];
	vec4 velocities[MAX_PARTICLES];

	float lambdas[MAX_PARTICLES];
	float densities[MAX_PARTICLES];
	float nearDensities[MAX_PARTICLES];

	uint usedCells;
	uint hashes[MAX_PARTICLES];
	uint hashTable[MAX_PARTICLES];
	uint cellEntries[MAX_PARTICLES];
	uint cells[MAX_PARTICLES * MAX_PARTICLES_PER_CELL];
} data;


flat out float vDepth;
out vec2 CenterOffset;
flat out float SmoothingRadius;


void main() {
	const vec2 vertexOffsets[4] = vec2[4](vec2(-1, 1), vec2(-1, -1), vec2(1, -1), vec2(1, 1));

	uint particleIndex = gl_InstanceID;
	vec4 center = View * vec4(data.positions[particleIndex].xyz, 1);

	// Also offset towards camera by smoothing radius for depth testing reasons
	//vec4 vPosition = center + vec4(vertexOffsets[gl_VertexID].xy * config.smoothingRadius, config.smoothingRadius, 0);
	// ^^^ The offset towards camera ends up screwing up the projection values ^^^

	//Depth = length(vPosition.xyz);

	vec4 vPosition = center + vec4(vertexOffsets[gl_VertexID].xy * config.smoothingRadius, 0, 0);

	vDepth = vPosition.z;
	CenterOffset = vertexOffsets[gl_VertexID].xy;
	SmoothingRadius = config.smoothingRadius;


	gl_Position = Projection * vPosition;
}