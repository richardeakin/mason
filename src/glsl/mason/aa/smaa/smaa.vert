#version 150

// Pass in the render target metrics as a uniform
uniform vec4 SMAA_RT_METRICS; // (1/w, 1/h, w, h)

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#define SMAA_GLSL_3 1
#include "SMAA.glsl"

uniform mat4 ciModelViewProjection;

in vec4      ciPosition;
in vec4      ciTexCoord0;

noperspective out vec2     vTexcoord;
noperspective out vec2     vPixcoord;
noperspective out vec4     vOffset[3];

void main()
{
	vTexcoord = vPixcoord = ciTexCoord0.st;

#if SMAA_PASS == 1
	SMAAEdgeDetectionVS( vTexcoord, vOffset );
#elif SMAA_PASS == 2
	SMAABlendingWeightCalculationVS( vTexcoord, vPixcoord, vOffset );
#elif SMAA_PASS == 3
	SMAANeighborhoodBlendingVS( vTexcoord, vOffset[0] );
#endif

	gl_Position = ciModelViewProjection * ciPosition;
}