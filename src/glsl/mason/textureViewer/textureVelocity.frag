#version 150

#include "mason/util.glsl"

uniform sampler2D uTex;
uniform float uScale;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	vec2 vel = texture( uTex, vTexCoord.st ).rg;

	// normalize by  2 * width
 	vel *= 2.0 / float( textureSize( uTex, 0 ).x );

 	// TODO: figure out a good consistent scale here. this is needed for LightingTest but not RayMarcher
 	vel *= 1000 * uScale;

	vec3 col = vec3( 0 );

	// TODO: get the -x's rendering correct here
	if( vel.x > 0.0 )
	    col.r = vel.x;
	else
	    col.g = - vel.x;

	if( vel.y > 0 )
	    col.b = vel.y;
	else {
		// -y: yellow
	    col.r += vel.y * -0.5;
	    col.g += vel.y * -0.5;
	}

	oFragColor = vec4( col, 1.0 );
}
