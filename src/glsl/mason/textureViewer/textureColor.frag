#version 150

#include "mason/util.glsl"

uniform sampler2D 	uTex0;
uniform float 		uScale = 1;
uniform bool 		uInverted = false;
uniform bool 		uFlipY = false;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	vec2 uv = vTexCoord.st;
	if( uFlipY ) {
		uv.y = 1.0 - uv.y;
	}

	vec4 col = texture( uTex0, uv );
	col.rgb *= uScale;
	if( uInverted ) {
		col.rgb = 1.0 - col.rgb;
	}
	oFragColor = col;

	// TODO: make this an option in the popup
	// - or might be able to do the same with blendmode when rendering to fbo
	oFragColor.a = 1.0;
}
