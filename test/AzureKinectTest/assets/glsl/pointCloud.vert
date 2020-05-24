#version 430

uniform mat4 ciModelViewProjection;
uniform mat3	ciNormalMatrix;

layout(binding = 0) uniform sampler2D uTexDepthBuffer;
layout(binding = 1) uniform sampler2D uTexDepthTable2dTo3d;

in vec4 ciPosition;
in vec4 ciColor;
in vec3 ciNormal;
in vec2 ciTexCoord0;

out vec4 vColor;
out vec3 vNormal;
out vec2 vTexCoord;

void main()
{	
	vColor = ciColor;
	vTexCoord = ciTexCoord0;

	// use gl_InstanceID to position square
	vec4 vertPos = ciPosition;
	vertPos.xyz *= 0.5; // scale entire cube

	ivec2 texSize = textureSize( uTexDepthBuffer, 0 );

	// vertPos.x += gl_InstanceID * 10;

	// use tex size to position x/y coords of each point
	ivec2 coord;
	coord.x = gl_InstanceID % texSize.x;
	coord.y = gl_InstanceID / texSize.y;

	// vertPos.x += coord.x * scale;
	// vertPos.y += coord.y * scale;

	// use gl_InstanceID to lookup in depth texture
	// TODO: need to convert this from depth to floating point?
	// - don't think so, the uint16_t value is in MM
	float depth = texelFetch( uTexDepthBuffer, coord, 0 ).r;
	if( depth == 0 ) {
		vertPos.xyz = vec3( 10e10 ); // cull off-screen
	}
	else {
		vec2 mapped = texelFetch( uTexDepthTable2dTo3d, coord, 0 ).rg;

		// TODO: these values are supposed to be mm, figure out why they are so small
		// TODO: multiply by depth value? fastpointcloud sample does this
		float scale = 100;
		// vertPos.x += mapped.x * scale * depth;
		// vertPos.y += mapped.y * scale * depth;

		vertPos.x += mapped.x * scale;
		vertPos.y += mapped.y * scale;

		vertPos.z += depth * 1000;

		// if( mapped.y > 1 ) {
		// 	vColor = vec4( 1, 0, 0, 1 );

		// }
	}


	vNormal		= ciNormalMatrix * ciNormal;
	gl_Position = ciModelViewProjection * vertPos;
}
