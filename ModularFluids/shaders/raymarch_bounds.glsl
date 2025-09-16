#version 430 core

#define MAX_PARTICLES 1024
#define MAX_CELL_COUNT 8192

in vec4 Position;

uniform vec2 ScreenSize;

layout(std140) uniform PVMatrices {
	mat4 View;
	mat4 Projection;
	mat4 ViewInverse;
	mat4 ProjectionInverse;
};

layout(binding = 1, std430) readonly restrict buffer FluidSimSSBO {
	vec4 simPosition;
	vec4 simBounds;
	ivec4 gridBounds;
	uint particleCount;
	float particleRadius;
	float cellSize;
	float padding;
	vec4 positions[MAX_PARTICLES];
	ivec2 hashList[MAX_PARTICLES];
	ivec2 lookupTable[MAX_CELL_COUNT];
};

layout(location = 0) out vec4 gpassAlbedoSpec;
layout(location = 1) out vec3 gpassPosition;
layout(location = 2) out vec3 gpassNormal;


// Spatial Grid functions

// All 'point' parameters are in sim-bounds space
bool isWithinBounds(vec3 point) {
	return !(any(lessThan(point, vec3(0))) || any(greaterThan(point, simBounds.xyz)));
}

ivec3 getCellCoords(vec3 point) {
	return ivec3(floor(point / cellSize));
}

int getCellHash(ivec3 cellCoords) {
	return cellCoords.x + (cellCoords.y * gridBounds.x) + (cellCoords.z * gridBounds.x * gridBounds.y);
}

bool isValidCell(ivec3 cellCoords) {
	return !(any(lessThan(cellCoords, ivec3(0))) || any(greaterThanEqual(cellCoords, ivec3(gridBounds.xyz))));
}

float densityKernel(float radius, float dist) {
	float value = 1 - (dist / radius);
	return value * value;
}

float sampleDensity(vec3 point) {
	// Necessary check because gridBounds could be slightly 'larger' than simBounds.
	if(!isWithinBounds(point)) {
		return -1.0;
	}

	ivec3 cellCoords = getCellCoords(point);

	float density = 0.0;
	for(int i = 0; i < 27; i++) {
		ivec3 offset = ivec3(i % 3, (i / 3) % 3, i / 9) - ivec3(1); // inefficient?
		ivec3 offsetCoords = ivec3(cellCoords) + offset;

		if(!isValidCell(offsetCoords)) continue; // Neighbouring cell is out of bounds

		int cellHash = getCellHash(offsetCoords);

		ivec2 indexLookup = ivec2(lookupTable[cellHash]);
		if(indexLookup.x == -1) continue; // Empty cell
	
		int startIndex = indexLookup.x;
		int endIndex = indexLookup.y;

		for(int n = startIndex; n < endIndex + 1; n++) {
			int particleIndex = hashList[n].x;
			vec3 toParticle = positions[particleIndex].xyz - point;
			float sqrDist = dot(toParticle, toParticle);
			
			// cellSize is smoothing radius
			if(sqrDist > cellSize * cellSize) continue;

			float dist = sqrt(sqrDist);
			density += densityKernel(cellSize, dist);
		}
	}
	return density;
}


// Returns approximate distance between ray origin and density iso-surface
float raymarchDensity(vec3 rayOrigin, vec3 rayDir, int maxSteps, float stepLength, float isoDensity) {
	// Apply a tiny little offset along the ray so it starts within the bounds of the box.
	const float startOffset = 0.0001;
	
	float accumulatedDensity = 0.0;
	for(int i = 0; i < maxSteps; i++) {
		vec3 stepPos = rayOrigin + rayDir * (stepLength * i + startOffset);
		float density = sampleDensity(stepPos);
		if(density == -1.0) {
			break;
		}

		accumulatedDensity += density;
		if(accumulatedDensity > isoDensity) {
			return stepLength * i;
		}
	}

	return -1.0;
}

vec3 densityGradient(vec3 point) {
	float dx = sampleDensity(point + vec3(0.001, 0, 0)) - sampleDensity(point + vec3(-0.001, 0, 0));
	float dy = sampleDensity(point + vec3(0, 0.001, 0)) - sampleDensity(point + vec3(0, -0.001, 0));
	float dz = sampleDensity(point + vec3(0, 0, 0.001)) - sampleDensity(point + vec3(0, 0, -0.001));
	
	return -vec3(dx, dy, dz);
}


// Raymarch settings
const int maxSteps = 32;
const float stepLength = 0.02;
const float densityThreshold = 1.0;


// crappy color parameters for testing
const vec4 mercury = vec4(vec3(0.5), 1.0);
const vec4 water = vec4(vec3(0.1, 0.5, 0.8), 0.3);


void main() {
	vec2 screenUVs = gl_FragCoord.xy / ScreenSize;
	vec2 ndc = screenUVs * 2 - 1;

	// View space
	vec3 vRayOrigin = vPosition;//(View * Position).xyz;
	vec3 vRayDirection = normalize((ProjectionInverse * vec4(ndc, 1, 1)).xyz);

	// World space
	vec3 rayOrigin = (ViewInverse * vec4(vPosition, 1)).xyz;//Position.xyz; // begins on surface of rasterized sim bounds
	vec3 rayDirection = (ViewInverse * vec4(vRayDirection, 0)).xyz;

	// Sim space (rotationally aligned with world space)
	vec3 sRayOrigin = rayOrigin - simPosition.xyz;

	float rayDistance = raymarchDensity(sRayOrigin, rayDirection, maxSteps, stepLength, densityThreshold);
	if(rayDistance == -1.0) discard;

	vec3 iso_vPos = vRayOrigin + vRayDirection * rayDistance;
	vec3 iso_pos = rayOrigin + rayDirection * rayDistance;

	gpassAlbedoSpec = water;
	gpassPosition = iso_vPos;
	gpassNormal = (View * vec4(normalize(densityGradient(iso_pos - simPosition.xyz)), 0)).xyz;

	vec4 clipPos = (Projection * vec4(iso_vPos, 1));
	float ndcPosZ = clipPos.z / clipPos.w;
	gl_FragDepth = ndcPosZ * 0.5 + 0.5;
}