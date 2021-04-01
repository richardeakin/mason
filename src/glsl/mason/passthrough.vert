#version 410

uniform mat4 ciModelViewProjection;

in vec4 ciPosition;
in vec2 ciTexCoord0;

out vec2 vTexCoord;

void main()
{	
	vTexCoord = ciTexCoord0;
	gl_Position = ciModelViewProjection * ciPosition;
}
