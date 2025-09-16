#version 430 core

#include "common.h"


in vec2 vTexCoord;

uniform sampler2D fluidDepthPass;

//layout(binding = PROJECTIONVIEW_UBO, std140) uniform PVMatrices {
//	mat4 View;
//	mat4 Projection;
//	mat4 ViewInverse;
//	mat4 ProjectionInverse;
//	vec4 CameraPos;
//};


layout(location = 0) out float smoothDepth;

//layout(depth_greater) out float gl_FragDepth;



void main() {
	const float gaussKernel[9] = float[9](
		1.0, 2.0, 1.0,
		2.0, 4.0, 2.0,
		1.0, 2.0, 1.0
	);

	// 5x5 Gauss Kernel
	const int kernelRadius = 4;
	const float sigmoidSpatial = 18.0;
	const float sigmoidRange = 12.0;

	// Precomputed terms
	const float stdDevSpatial = 2.0 * sigmoidSpatial * sigmoidSpatial;
	const float stdDevRange = 2.0 * sigmoidRange * sigmoidRange;

	ivec2 kernelCoords = ivec2(gl_FragCoord.xy - 0.5);
	float kernelValue = texelFetch(fluidDepthPass, kernelCoords, 0).r;
	

	// Bilateral gaussian kernel filter
	float gaussSum = 0.0;
	float weightSum = 0.0;
	for(int y = -kernelRadius; y <= kernelRadius; y++) {
		for(int x = -kernelRadius; x <= kernelRadius; x++) {
			ivec2 pixelOffset = ivec2(x, y);
			float sampledValue = texelFetchOffset(fluidDepthPass, kernelCoords, 0, pixelOffset).r;
			float valueDiff = (kernelValue - sampledValue);

			float weight = exp(-(dot(pixelOffset, pixelOffset)/stdDevSpatial) - ((valueDiff * valueDiff)/stdDevRange));

			gaussSum += sampledValue * weight;
			weightSum += weight;
		}
	}
	
	// Divide by total weight to normalize kernel
	smoothDepth = gaussSum / weightSum;
}