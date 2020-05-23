#version 410

uniform mat4 ciModelViewProjection;
uniform mat3	ciNormalMatrix;

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

	vNormal		= ciNormalMatrix * ciNormal;
	gl_Position = ciModelViewProjection * ciPosition;
}
