#version 150

#include "mason/util.glsl"

uniform sampler2D uTex0;
uniform float 	uScale;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	float depth = texture( uTex0, vTexCoord.st ).r;

	depth = 1.0 - depth;

	float distStart = 0.00;
	float distRange = 0.03;

	depth = range( depth, distStart, distRange, 0.0, 1.0 );

	depth = pow( depth, 0.6 ); // increase visibility
	depth *= uScale;

	vec3 col = vec3( depth, depth, depth );

	if( depth > distStart && depth < ( distStart + distRange ) ) {
		// col = vec3( 1, 0, 0 );
	}

	oFragColor = vec4( col, 1.0 );
}
