#version 430 core

#include "common.h"


flat in float vDepth;
in vec2 CenterOffset;
flat in float SmoothingRadius;


layout(binding = PROJECTIONVIEW_UBO, std140) uniform PVMatrices {
	mat4 View;
	mat4 Projection;
	mat4 ViewInverse;
	mat4 ProjectionInverse;
	vec4 CameraPos;
};

layout(location = 0) out float fluidDepth;

//layout(depth_greater) out float gl_FragDepth;

float convertToNdcDepth(float viewDepth) {
	float clipZ = viewDepth * Projection[2].z + Projection[3].z;
	return (clipZ / -viewDepth) * 0.5 + 0.5;
}



void main() {
	float sqrDist = dot(CenterOffset, CenterOffset);
	if(sqrDist > 1) discard;

	// distance^2 + height^2 = radius^2
	// height = sqrt(radius^2 - distance^2)
	//float depthOffset = (1.0 - sqrt(1.0 - sqrDist)) * SmoothingRadius;
	
	float depthOffset = sqrt(1.0 - sqrDist) * SmoothingRadius;

	float vDepthMin = vDepth + depthOffset;
	fluidDepth = convertToNdcDepth(vDepthMin);
	gl_FragDepth = fluidDepth;
}