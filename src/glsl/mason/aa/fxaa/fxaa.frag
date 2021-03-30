#version 150

#ifdef GL_ARB_gpu_shader5
#extension GL_ARB_gpu_shader5 : enable
#else
#extension GL_EXT_gpu_shader4 : enable
#define FXAA_FAST_PIXEL_OFFSET 0
#endif

#define FXAA_PC 1
#define FXAA_GLSL_130 1			// GLSL version 130 and later
#define FXAA_QUALITY__PRESET 39 // EXTREME QUALITY
#define FXAA_GREEN_AS_LUMA 0 	// if 0, luma MUST be packed in uTexture.a

#include "FXAA3_11.glsl"

uniform sampler2D	uTexture;
uniform vec4		uExtents; // .x = 1.0/screenWidthInPixels, .y = 1.0/screenHeightInPixels

// Choose the amount of sub-pixel aliasing removal.
// This can effect sharpness.
//   1.00 - upper limit (softer)
//   0.75 - default amount of filtering
//   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
//   0.25 - almost off
//   0.00 - completely off
uniform float uQualitySubpix = 0.75;

// The minimum amount of local contrast required to apply algorithm.
//   0.333 - too little (faster)
//   0.250 - low quality
//   0.166 - default
//   0.125 - high quality
//   0.033 - very high quality (slower)
uniform float uQualityEdgeThreshold = 0.033;


out     vec4        oFragColor;

void main()
{
	FxaaFloat2 fxaaQualityRcpFrame;
	fxaaQualityRcpFrame.x = uExtents.x;
	fxaaQualityRcpFrame.y = uExtents.y;

	FxaaFloat2 uv = gl_FragCoord.xy * uExtents.xy;


	// You dont need to touch theses variables it have no visible effect
	FxaaFloat QualityEdgeThresholdMin = 0.0;
	FxaaFloat ConsoleEdgeSharpness = 8.0;
	FxaaFloat ConsoleEdgeThreshold = 0.125;
	FxaaFloat ConsoleEdgeThresholdMin = 0.05;
	const FxaaFloat4 notUsed = FxaaFloat4( 0.0 );

	oFragColor = FxaaPixelShader( uv, notUsed, uTexture, uTexture, uTexture, fxaaQualityRcpFrame,
		notUsed, notUsed, notUsed, 
		uQualitySubpix, uQualityEdgeThreshold, QualityEdgeThresholdMin, 
		ConsoleEdgeSharpness, ConsoleEdgeThreshold, ConsoleEdgeThresholdMin, notUsed
	);

	// oFragColor = texture( uTexture, uv ); // testing: output original

	// set alpha back to 1.0 (uTexture.a contained rgb's luma value)
	oFragColor.a = 1.0;
}
