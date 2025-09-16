#version 430 core

out vec2 vTexCoord;

void main() {
	vec2 verts[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
	gl_Position = vec4(verts[gl_VertexID], 0, 1);
	vTexCoord = 0.5 * gl_Position.xy + vec2(0.5);
}