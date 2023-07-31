#version 430

#include "mason/util.glsl"

uniform sampler2D 	uTex0;
uniform float 		uScale = 1.0;
uniform bool 		uInverted = false;
uniform bool 		uFlipY = false;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	vec2 uv = vTexCoord.st;
	if( uFlipY ) {
		uv.y = 1.0 - uv.y;
	}

	float depth = texture( uTex0, uv ).r;

	// TODO: this neds to be more flexible
	// - tweaked it now so I can use with the kinect buffer, but this also gets
	//   used for viewing the z-buffer
	if( uInverted ) {
		depth = 1.0 - depth;

		float distStart = 0.00;
		float distRange = 0.03;
		depth = range( depth, distStart, distRange, 0.0, 1.0 );
	}

	depth = pow( depth, 0.6 ); // increase visibility
	depth *= uScale;

	vec3 col = vec3( depth, depth, depth );

	// if( depth > distStart && depth < ( distStart + distRange ) ) {
		// col = vec3( 1, 0, 0 );
	// }

	oFragColor = vec4( col, 1.0 );
}
