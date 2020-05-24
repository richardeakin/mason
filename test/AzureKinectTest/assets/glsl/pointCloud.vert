#version 430

uniform mat4 ciModelViewProjection;
uniform mat3	ciNormalMatrix;

layout(location = 0) uniform sampler2D uTexDepthBuffer;
layout(location = 1) uniform sampler2D uTexDepthTable2dTo3d;

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
	vertPos.xyz *= 0.5;

	ivec2 texSize = textureSize( uTexDepthBuffer, 0 );

	// vertPos.x += gl_InstanceID * 10;

	// use tex size to position x/y coords of each point
	ivec2 coord;
	coord.x = gl_InstanceID % texSize.x;
	coord.y = gl_InstanceID / texSize.y;

	float scale = 1;
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

		// TODO: multiply by depth value? fastpointcloud sample does this
		vertPos.x += mapped.x * 1000 * depth;
		vertPos.y += mapped.y * 1000 * depth;
		// vertPos.x += mapped.x * depth;
		// vertPos.y += mapped.y * depth;
		vertPos.z += depth * 1000; // TODO: convert mm to cm only
	}

	vNormal		= ciNormalMatrix * ciNormal;
	gl_Position = ciModelViewProjection * vertPos;
}
