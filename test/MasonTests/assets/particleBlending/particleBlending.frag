#version 410

//#include "util.glsl"

uniform float   uTime;
uniform vec4    uColor;

out vec4	oFragColor;

void main()
{
//	oFragColor = vec4( 0.5, 0, 0, 1 );
//	return;

	vec2 p = gl_PointCoord.xy - vec2( 0.5, 0.5 );
	float r = sqrt( dot( p, p ) );

	if( r > 0.5 )
		discard;

	vec3 col = uColor.rgb;
	float alpha = uColor.a;

	float innerSmoothRadius = 0.4; // max is 0.5

	// add center gradient for tentacles
	// smoothen edge of circle
	alpha *= 1 - smoothstep( innerSmoothRadius, 0.5, r );

	oFragColor.rgb = col * alpha;
	oFragColor.a = alpha;
}
