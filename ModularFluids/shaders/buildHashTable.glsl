#version 460

//#define MAX_PARTICLES 2048
#define MAX_PARTICLES 65536
//#define MAX_PARTICLES 262144

//#define MAX_PARTICLES_PER_CELL 16 // Only viable when using Mullet.M position based fluid technique
#define MAX_PARTICLES_PER_CELL 32

#define WORKGROUP_SIZE_X 1024

#define COMPUTE_CELLS_PER_WORKGROUP 16

#define PROJECTIONVIEW_UBO 0

#define FLUID_CONFIG_UBO 1
#define FLUID_DATA_SSBO 2


layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;


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

	readonly float lambdas[MAX_PARTICLES];
	readonly float densities[MAX_PARTICLES];
	readonly float nearDensities[MAX_PARTICLES];

	readonly uint usedCells;
	readonly uint hashes[MAX_PARTICLES];
	readonly uint hashTable[MAX_PARTICLES];
	uint cellEntries[MAX_PARTICLES];
	writeonly uint cells[];
} data;

layout(binding = 3, std430) writeonly restrict buffer DispatchIndirectCommand {
	uint num_groups_x;
	uint num_groups_y;
	uint num_groups_z;
} indirectCmd;



void main() {
    uint particleIndex = gl_GlobalInvocationID.x;
	if(particleIndex >= config.particleCount) return;


    uint cellHash = data.hashes[particleIndex];
    uint cellIndex = data.hashTable[cellHash];

	uint cellEntryCount = atomicAdd(data.cellEntries[cellIndex], 1);
	uint cellEntryIndex = cellIndex * MAX_PARTICLES_PER_CELL + cellEntryCount;
	
	data.cells[cellEntryIndex] = particleIndex;

	uint dispatchCount = (data.usedCells / COMPUTE_CELLS_PER_WORKGROUP) + uint((data.usedCells % COMPUTE_CELLS_PER_WORKGROUP) != 0);

	indirectCmd.num_groups_x = dispatchCount;
    indirectCmd.num_groups_y = uint(dispatchCount != 0);
	indirectCmd.num_groups_z = uint(dispatchCount != 0);
}