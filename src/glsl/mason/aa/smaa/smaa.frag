#version 150

// Pass in the render target metrics as a uniform
uniform vec4 SMAA_RT_METRICS; // (1/w, 1/h, w, h)

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#define SMAA_GLSL_3 1
#include "SMAA.glsl"

noperspective in vec2    vTexcoord;
noperspective in vec2    vPixcoord;
noperspective in vec4    vOffset[3];

out vec4 oFragColor;

#if SMAA_PASS == 1

uniform sampler2D uColorTex;

void main()
{
    oFragColor = vec4( SMAALumaEdgeDetectionPS( vTexcoord, vOffset, uColorTex ), 0, 0 );
}

#elif SMAA_PASS == 2

uniform sampler2D uEdgesTex;
uniform sampler2D uAreaTex;
uniform sampler2D uSearchTex;

void main()
{
	ivec4 subsampleIndices = ivec4( 0 );
	oFragColor = SMAABlendingWeightCalculationPS( vTexcoord, vPixcoord, vOffset, uEdgesTex, uAreaTex, uSearchTex, subsampleIndices );
}

#elif SMAA_PASS == 3

uniform sampler2D uColorTex;
uniform sampler2D uBlendTex;

void main()
{
	oFragColor = SMAANeighborhoodBlendingPS( vTexcoord, vOffset[0], uColorTex, uBlendTex );

	// set alpha back to 1.0 (uTexture.a contained rgb's luma value)
	oFragColor.a = 1.0;
}

#endif
