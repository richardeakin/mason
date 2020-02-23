#version 150

#include "mason/util.glsl"

uniform sampler2D uTex0;
uniform float 	uScale = 1;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	vec4 col = texture( uTex0, vTexCoord.st );
	col.rgb *= uScale;
	oFragColor = col;

	// TODO: make this an option in the popup
	// - or might be able to do the same with blendmode when rendering to fbo
	oFragColor.a = 1.0;
}
