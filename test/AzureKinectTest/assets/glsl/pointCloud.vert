#version 430

uniform mat4 ciModelViewProjection;
uniform mat3	ciNormalMatrix;

uniform sampler2D uTexDepthBuffer;

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

	// TODO NEXT: use tex size to properly position x/y coords of each point
	ivec2 coord;
	coord.x = gl_InstanceID % texSize.x;
	coord.y = gl_InstanceID / texSize.y;

	float scale = 1;
	vertPos.x += coord.x * scale;
	vertPos.y += coord.y * scale;

	// TODO: use gl_InstanceID to lookup in depth texture
	// TODO: need to convert this from depth to floating point
	float depth = texelFetch( uTexDepthBuffer, coord, 0 ).r;
	if( depth == 0 ) {
		vertPos.xyz = vec3( 10e10 );
	}
	else {
		vertPos.z += depth * 10000;
	}

	vNormal		= ciNormalMatrix * ciNormal;
	gl_Position = ciModelViewProjection * vertPos;
}
