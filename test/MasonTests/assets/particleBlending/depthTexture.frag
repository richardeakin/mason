#version 150

uniform sampler2D uTex0;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	float depth = texture( uTex0, vTexCoord.st ).r;

	depth = 1.0 - depth;

	depth = pow( depth, 0.65 ); // increase visibility

	oFragColor = vec4( depth, depth, depth, 1.0 );
}
