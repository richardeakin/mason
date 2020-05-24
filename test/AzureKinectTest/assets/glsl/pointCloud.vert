#version 430

uniform mat4 	ciModelViewProjection;
uniform mat3	ciNormalMatrix;

layout(binding = 0) uniform usampler2D uTexDepthBuffer;
layout(binding = 1) uniform sampler2D uTexDepthTable2dTo3d;

uniform vec3 uDevicePos;

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
	vertPos.xyz *= 0.2; // scale entire cube

	ivec2 texSize = textureSize( uTexDepthBuffer, 0 );

	// use tex size to position x/y coords of each point
	ivec2 coord;
	coord.x = gl_InstanceID % texSize.x;
	coord.y = gl_InstanceID / texSize.y;

	uint depthMM = texelFetch( uTexDepthBuffer, coord, 0 ).r;
	if( depthMM == 0 ) {
		vertPos.xyz = vec3( 10e10 ); // cull off-screen
	}
	else {
		float depth = float( depthMM ) * 0.1;

		vec2 mapped = texelFetch( uTexDepthTable2dTo3d, coord, 0 ).rg;
		mapped *= -1; // this along with device pos offset matches CaptureAzureKinect::fillBodyFromSkeleton()

		vertPos.x += mapped.x * depth;
		vertPos.y += mapped.y * depth;

		vertPos.z += depth;

		vertPos.xyz += uDevicePos;
	}


	vNormal		= ciNormalMatrix * ciNormal;
	gl_Position = ciModelViewProjection * vertPos;
}
