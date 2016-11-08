#version 410

//#include "util.glsl"

uniform float   uTime;

uniform mat4	ciModelViewProjection;
uniform mat4	ciModelView;
uniform mat4	ciProjectionMatrix;

layout(location = 0) in vec4 iPosition;
layout(location = 1) in vec4 iLookups;
layout(location = 2) in vec4 iMovement;

uniform float ciElapsedSeconds;

void main()
{
	float pointSize = 100;

	vec3 pos = iPosition.xyz;

	// scale point size based on distance to eye
	{
		vec4 eyePosition = ciModelView * vec4( pos, 1.0 );
		vec4 projection = ciProjectionMatrix * vec4( 1.0, 1.0, eyePosition.z, eyePosition.w );
		pointSize *= projection.x / projection.w;
	}

    gl_PointSize = pointSize;
	gl_Position	= ciModelViewProjection * vec4( pos, 1 );
}
