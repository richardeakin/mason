#version 410

//#include "util.glsl"

uniform float   uTime;

out vec4	oFragColor;

void main()
{
//	oFragColor = vec4( 0.5, 0, 0, 1 );
//	return;

	vec3 col = vec3( 0.2, 1, 1 );
	float alpha = 0.5;

//	float innerSmoothRadius = 0.4; // max is 0.5

	// add center gradient for tentacles
	// smoothen edge of circle
//	alpha *= 1 - smoothstep( innerSmoothRadius, 0.5, r );

	oFragColor.rgb = col * alpha;
	oFragColor.a = alpha;
}
