#version 410

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelView;
uniform mat4 ciModelMatrix;

in vec4 ciPosition;
in vec4 ciColor;

out vec4 vViewPosition;
out vec4 vWorldPosition;
out vec4 vColor;

void main()
{	
	vViewPosition = ciModelView * ciPosition;
	vWorldPosition = ciModelMatrix * ciPosition;
	vColor = ciColor;
	gl_Position = ciModelViewProjection * ciPosition;
}
