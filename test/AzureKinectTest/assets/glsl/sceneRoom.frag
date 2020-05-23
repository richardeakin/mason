#version 430

in vec4 vViewPosition;
in vec4 vWorldPosition;
in vec4	vColor;

out vec4 oFragColor;

uniform float uBodyMaxDistance = -1;

vec3 applyFog( in vec3 rgb, in float dist )
{
	const float 	fogDistanceStart = 0;
	const  float	fogBias  = 0.0013;

	float fogAmount = clamp( 1.0 - exp( -( dist - fogDistanceStart ) * fogBias ), 0.0, 1.0 );
	vec3  fogColor = vec3( 0, 0.03, 0.1 );
	return mix( rgb, fogColor, fogAmount );
}

void main()
{
	vec4 col = vColor;

    col.rgb = applyFog( col.rgb, abs( vViewPosition.z ) );

	if( uBodyMaxDistance > 0 ) {
		float x = smoothstep( 0.0, 5.0, vWorldPosition.z );
		x *= 1.0 - smoothstep( uBodyMaxDistance, uBodyMaxDistance + 5.0, length( vWorldPosition.xz ) );
		col.rgb = mix( col.rgb, col.rgb + vec3( 0.2, 0.32, 0.4 ), x );
	}

	oFragColor = col;
}
