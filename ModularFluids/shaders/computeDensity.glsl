layout(local_size_x = COMPUTE_CELLS_PER_WORKGROUP, local_size_y = MAX_PARTICLES_PER_CELL, local_size_z = 1) in;

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

layout(binding = FLUID_DATA_SSBO, std430) restrict buffer FluidData {
	readonly vec4 positions[MAX_PARTICLES];
	readonly vec4 previousPositions[MAX_PARTICLES];
	readonly vec4 velocities[MAX_PARTICLES];

	writeonly float lambdas[MAX_PARTICLES];
	writeonly float densities[MAX_PARTICLES]; // These
	writeonly float nearDensities[MAX_PARTICLES]; // Ones

	readonly uint usedCells;
	readonly uint hashes[MAX_PARTICLES];
	readonly uint hashTable[MAX_PARTICLES];
	readonly uint cellEntries[MAX_PARTICLES];
	readonly uint cells[];
} data;


const float PI = acos(-1.f);
const float sqrSmoothingRadius = config.smoothingRadius * config.smoothingRadius;


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
const float normFactor_P6 = 315.f / (64.f * PI * pow(config.smoothingRadius, 9));
const float normFactor_S = 45.f / (PI * pow(config.smoothingRadius, 6));

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


const float epsilon = 0.4f;

// Calculates lambda to solve density constraint
void calculateLambda(uint particleIndex, out float lambda) {
	ivec3 cellCoords = getCellCoords(data.positions[particleIndex].xyz);

	float constraintGradient = 0.f;

	float localDensity = 0.f;
	float localDensityGradient = 0.f;
	for (uint i = 0; i < 27; i++) {
		ivec3 offset = ivec3(i % 3, (i / 3) % 3, i / 9) - ivec3(1);
		ivec3 offsetCellCoords = cellCoords + offset;
		
		uint cellHash = getCellHash(offsetCellCoords);
		uint cellIndex = data.hashTable[cellHash];
		if(cellIndex == 0xFFFFFFFF) continue;

		uint entries = data.cellEntries[cellIndex];

		for (uint n = 0; n < entries; n++) {
			uint cellEntryIndex = cellIndex * MAX_PARTICLES_PER_CELL + n;
			uint otherParticleIndex = data.cells[cellEntryIndex];

			vec3 toParticle = data.positions[otherParticleIndex].xyz - data.positions[particleIndex].xyz;
			float sqrDist = dot(toParticle, toParticle);

			if (sqrDist >= sqrSmoothingRadius) continue;

			localDensity += config.particleMass * polySixKernel(sqrDist);
			
			float dist = sqrt(sqrDist);

			float densityDerivative = config.particleMass * spikyKernelGradient(dist);
			localDensityGradient += densityDerivative;

			if (particleIndex == otherParticleIndex) continue;
			
			constraintGradient -= densityDerivative * densityDerivative;
		}
	}

	constraintGradient += (localDensityGradient * localDensityGradient);
	constraintGradient /= (config.restDensity * config.restDensity);

	float densityConstraint = (localDensity / config.restDensity) - 1.f;

	lambda = -densityConstraint / (constraintGradient + epsilon);
}


// Clavet.S double-density
// Density kernels
float densityKernel(float radius, float dist) {
	float value = 1.f - (dist / radius);
	return value * value;
}

float nearDensityKernel(float radius, float dist) {
	float value = 1.f - (dist / radius);
	return value * value * value;
}

// Calculates density at specified particle position
void calculateDensity(uint particleIndex, out float density, out float nearDensity) {
	ivec3 cellCoords = getCellCoords(data.positions[particleIndex].xyz);

	density = 0.f;
	nearDensity = 0.f;
	for (uint i = 0; i < 27; i++) {
		ivec3 offset = ivec3(i % 3, (i / 3) % 3, i / 9) - ivec3(1);
		ivec3 offsetCellCoords = cellCoords + offset;
		
		uint cellHash = getCellHash(offsetCellCoords);
		uint cellIndex = data.hashTable[cellHash];
		if(cellIndex == 0xFFFFFFFF) continue;

		uint entries = data.cellEntries[cellIndex];

		for (uint n = 0; n < entries; n++) {
			uint cellEntryIndex = cellIndex * MAX_PARTICLES_PER_CELL + n;
			uint otherParticleIndex = data.cells[cellEntryIndex];

			vec3 toParticle = data.positions[otherParticleIndex].xyz - data.positions[particleIndex].xyz;
			float sqrDist = dot(toParticle, toParticle);

			if (sqrDist > sqrSmoothingRadius) continue;

			float dist = sqrt(sqrDist);
			density += densityKernel(config.smoothingRadius, dist);
			nearDensity += nearDensityKernel(config.smoothingRadius, dist);
		}
	}
}


void main() {
	uint cellIndex = gl_GlobalInvocationID.x;
	uint entryIndex = gl_LocalInvocationID.y;

	bool isValidThread = (cellIndex < data.usedCells && entryIndex < data.cellEntries[cellIndex]);
	if (!isValidThread) return;

	uint cellEntryIndex = cellIndex * MAX_PARTICLES_PER_CELL + entryIndex;
	uint particleIndex = data.cells[cellEntryIndex];


	// Mullen.M
	float lambda;
	calculateLambda(particleIndex, lambda);

	data.lambdas[particleIndex] = lambda;


	// Clavet.S
//	float density;
//	float nearDensity;
//
//	calculateDensity(particleIndex, density, nearDensity);
//
//	data.densities[particleIndex] = density;
//	data.nearDensities[particleIndex] = nearDensity;


}