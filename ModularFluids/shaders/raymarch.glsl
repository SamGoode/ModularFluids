


in vec2 vTexCoord;

uniform sampler2D fluidDepthPass;
uniform sampler2D smoothDepthPass;

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
	uint cells[];
} data;


layout(location = 0) out vec4 gpassAlbedoSpec;
layout(location = 1) out vec3 gpassPosition;
layout(location = 2) out vec3 gpassNormal;

const float PI = acos(-1.f);
const float sqrSmoothingRadius = config.smoothingRadius * config.smoothingRadius;


// All 'point' parameters are in world space

// Spatial hashing
ivec3 getCellCoords(vec3 point) {
	return ivec3(floor(point / config.smoothingRadius));
}

uint getCellHash(ivec3 cellCoords) {
	// Three large prime numbers (from the brain of Matthias Teschner)
	const uint p1 = 73856093;
	const uint p2 = 19349663; // Apparently this one isn't prime
	const uint p3 = 83492791;

	return ((p1 * uint(cellCoords.x)) ^ (p2 * uint(cellCoords.y)) ^ (p3 * uint(cellCoords.z))) % MAX_PARTICLES;
}


// Mullen.M
// Kernel normalization factors
const float normFactor_P6 = 315.f / ((64.f * PI) * pow(config.smoothingRadius, 9));
const float normFactor_S = 45.f / ((PI) * pow(config.smoothingRadius, 6));

// Density kernels
// scaled smoothing radius so kernel has curve of radius=1.
float polySixKernel(float sqrDist) {
	//float scaledSqrDist = sqrDist/sqrSmoothingRadius;
	//float value = 1.f - scaledSqrDist;
	float value = sqrSmoothingRadius - sqrDist;
	return value * value * value * normFactor_P6;
}

float spikyKernelGradient(float dist) {
	//float scaledDist = dist/config.smoothingRadius;
	//float value = 1.f - scaledDist;
	float value = config.smoothingRadius - dist;
	return value * value * normFactor_S;
}


// Clavet.S density kernel
float densityKernel(float radius, float dist) {
	float value = 1 - (dist / radius);
	return value * value;
}


// Sample density with neighbourhood search
float sampleDensity(vec3 point) {
	ivec3 cellCoords = getCellCoords(point);

	float density = 0.0;
	for(uint i = 0; i < 27; i++) {
		ivec3 offset = ivec3(i % 3, (i / 3) % 3, i / 9) - ivec3(1); // inefficient?
		ivec3 offsetCoords = ivec3(cellCoords) + offset;

		uint cellHash = getCellHash(offsetCoords);
		uint cellIndex = data.hashTable[cellHash];

		if(cellIndex == 0xFFFFFFFF) continue;

		uint entries = data.cellEntries[cellIndex];

		for(uint n = 0; n < entries; n++) {
			uint particleIndex = data.cells[(cellIndex * MAX_PARTICLES_PER_CELL) + n];
			vec3 toParticle = data.positions[particleIndex].xyz - point;
			
			float sqrDist = dot(toParticle, toParticle);
			if(sqrDist > sqrSmoothingRadius) continue;

			density += config.particleMass * polySixKernel(sqrDist);
			
			// float dist = sqrt(sqrDist);
			// density += config.particleMass * densityKernel(config.smoothingRadius, dist);
		}
	}
	return density;
}

// Sample density gradient with neighbourhood search
vec3 sampleDensityGradient(vec3 point) {
	ivec3 cellCoords = getCellCoords(point);

	vec3 gradientSum = vec3(0);
	for(uint i = 0; i < 27; i++) {
		ivec3 offset = ivec3(i % 3, (i / 3) % 3, i / 9) - ivec3(1); // inefficient?
		ivec3 offsetCoords = ivec3(cellCoords) + offset;

		uint cellHash = getCellHash(offsetCoords);
		uint cellIndex = data.hashTable[cellHash];

		if(cellIndex == 0xFFFFFFFF) continue;

		uint entries = data.cellEntries[cellIndex];

		for(uint n = 0; n < entries; n++) {
			uint particleIndex = data.cells[(cellIndex * MAX_PARTICLES_PER_CELL) + n];
			vec3 toParticle = data.positions[particleIndex].xyz - point;
			
			float sqrDist = dot(toParticle, toParticle);
			if(sqrDist > sqrSmoothingRadius) continue;

			float dist = sqrt(sqrDist);
			vec3 dir = dist > 0 ? toParticle / dist : vec3(0);

			gradientSum -= dir * config.particleMass * spikyKernelGradient(dist);
		}
	}
	return gradientSum;
}


// Returns approximate distance between ray origin and density iso-surface
float raymarchDensity(vec3 rayOrigin, vec3 rayDir, int maxSteps, float stepLength, float isoDensity, float startDepth) {
	float accumulatedDensity = 0.0;
	for(uint i = 0; i < maxSteps; i++) {
		vec3 stepPos = rayOrigin + rayDir * (stepLength * i + startDepth);

		float density = sampleDensity(stepPos);
		accumulatedDensity += density;

		if(accumulatedDensity > isoDensity) {
			return stepLength * i + startDepth;
		}
	}

	return -1.0;
}


vec3 getViewPos(vec2 uv, float depth) {
	vec4 projectedCoords = ProjectionInverse * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1);
	projectedCoords = projectedCoords / projectedCoords.w;

	return projectedCoords.xyz;
}

float convertToNdcDepth(float viewDepth) {
	float clipZ = viewDepth * Projection[2].z + Projection[3].z;
	return (clipZ / -viewDepth) * 0.5 + 0.5;
}

vec3 getScreenNormal(sampler2D depthTex, vec2 UVs) {
	float depth = texture(depthTex, UVs).r;
	vec3 viewPos = getViewPos(UVs, depth);

	return normalize(cross(dFdx(viewPos), dFdy(viewPos)));
}


// Raymarch parameters
const int maxSteps = 10;
const float stepLength = 0.02f;
const float isoDensity = 1.f;


// crappy color parameters for testing
const vec4 mercury = vec4(vec3(0.5), 1.0);
const vec4 water = vec4(vec3(0.1, 0.5, 0.8), 0.3);


void main() {
//	 // Rasterized spheres
//	 float minDepth = texture(fluidDepthPass, vTexCoord).r;
//	 float smoothedDepth = texture(smoothDepthPass, vTexCoord).r;
//
//	 vec3 viewPos = getViewPos(vTexCoord, minDepth);
//	 vec3 smoothedPos = getViewPos(vTexCoord, smoothedDepth);
//
//	 gpassAlbedoSpec = water;
//	 gpassPosition = viewPos;
//	 gpassNormal = getScreenNormal(smoothDepthPass, vTexCoord);
//
//	 gl_FragDepth = convertToNdcDepth(viewPos.z);


	// Raymarching SPH density field
	float minDepth = texture(fluidDepthPass, vTexCoord).r;

	vec3 vRayOrigin = getViewPos(vTexCoord, minDepth);
	vec3 vRayDirection = vec3(vRayOrigin.xy / -vRayOrigin.z, -1);
	vec3 rayDirection = (ViewInverse * vec4(vRayDirection, 0)).xyz;

	float startDepth = abs(vRayOrigin.z);

	float rayDistance = raymarchDensity(CameraPos.xyz, rayDirection, maxSteps, stepLength, isoDensity, startDepth);
	if(rayDistance == -1.0) discard;

	vec3 iso_vPos = vRayDirection * rayDistance;
	vec3 iso_pos = CameraPos.xyz + rayDirection * rayDistance;

	// Blend density gradient and screen normal to estimate normal at iso-surface.
	vec3 gradientNormal = (View * vec4(normalize(sampleDensityGradient(iso_pos)), 0)).xyz;
	vec3 screenNormal = getScreenNormal(smoothDepthPass, vTexCoord);

	// Formula to calculate particle screen space size at given depth
	float R = (900.f * config.smoothingRadius) / (2.f * abs(rayDistance) * tan(PI * 0.25f));
	
	// Arbitrary parameters to adjust the blending normal weights
	const float A = 0.9f;
	const float B = 0.6f;

	float weight1 = A * length(iso_vPos - vRayOrigin);
	float weight2 = exp(B * R);
	float weight = min(weight1 * weight2, 1);

	gpassAlbedoSpec = water;
	gpassPosition = iso_vPos;
	gpassNormal = (gradientNormal * weight) + screenNormal * (1 - weight);
	//gpassNormal = gradientNormal;
	//gpassNormal = screenNormal;

	gl_FragDepth = convertToNdcDepth(-rayDistance);
}