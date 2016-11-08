#version 410

#include "util.glsl"
#include "transform.glsl"

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelView;

uniform float 	uTime;
uniform float 	uDeltaTime;
uniform vec3 	uPosA;

layout(location = 0) in vec4 iPosition;
layout(location = 1) in vec4 iLookups;
layout(location = 2) in vec4 iMovement;

out vec4 oPosition;
out vec4 oLookups;
out vec4 oMovement;

void main()
{
	// ----------------------
	// unpack feedback data
	vec3 pos = iPosition.xyz;

	float age = iLookups.x;
	float maxAge = iLookups.y;
	float particleN = iLookups.z;
	// ----------------------

	vec3 centerPos = vec3( 0 );

	if( age < 0.01 ) {
		pos = centerPos;
	}

    float n = particleN * 0.5;
    float t = uTime * 0.07;
	vec3 noisyPos = vec3(
		noise( vec2( n * -1.3 + 1.2 * t, n * 1.1 + t * 0.2323  ) ),
		noise( vec2( n * 11.3 + 2.1 * t, n * 7.3 + t * 0.1 ) ),
		noise( vec2( n * 0.3 + t, n + t ) )
	);

	noisyPos = noisyPos * 2.0 - 1.0;
	noisyPos *= 0.12;

	pos += ( centerPos - pos ) * 0.9 + noisyPos * 2.1;

	age += uDeltaTime;

	// ----------------------
	// pack feedback data
	oPosition.xyz = pos;
//	oPosition.w = flag;

	oLookups.x = age;
	oLookups.y = maxAge;
	oLookups.z = particleN;
	oLookups.w = 0;

	oMovement = vec4( 0 );
	// ----------------------
}
