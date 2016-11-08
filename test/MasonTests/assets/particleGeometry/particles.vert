#version 410

//uniform float   uTime;

uniform mat4	ciModelViewProjection;
uniform mat4	ciModelView;
uniform mat4	ciProjectionMatrix;

layout(location = 0) in vec4 iPosition;
//layout(location = 1) in vec4 iLookups;
layout(location = 2) in vec4 iMovement;

uniform float ciElapsedSeconds;

out vec4 vPositionWorld;
out vec4 vMovement;

void main()
{
	vec3 pos = iPosition.xyz;
	vPositionWorld = vec4( pos, 1 );
	vMovement = iMovement;
	gl_Position	= ciModelViewProjection * vec4( pos, 1 );
}
