// Old postprocess shader that was trying to combine DoF and fog in a single pass
// Currently defunct.

#version 400

#include "mason/post/toneMap.glsl"

#define AA_NONE 0
#define AA_MSAA 1
#define AA_FXAA 2
#define AA_SMAA 3

#ifndef AA_TYPE
#define AA_TYPE AA_NONE // disabled
#endif

uniform mat4 uProjectionMatrixInverse;

uniform sampler2D uTexColor;
uniform sampler2D uTexDepth;
uniform sampler2D uTexGlow;
uniform sampler2D uTexVelocity;

uniform vec2 uProjectionParams; // needed to compute linear depth

in vec2	vTexCoord;
out vec4 oFragColor;

uniform float uExposure = 1.0; // hud: "tone-mapping exposure", step = 0.02

uniform bool uFogEnabled = false;
float fogDensity = 0.000002;
float distStart = 20.0;
float distRange = 300.0;
uniform vec3 fogColor = vec3( 0.1, 0.25, 0.35 );
//vec3 fogColor = vec3( 0 );

float getLinearDepth( float depth )
{
  return uProjectionParams.y / ( depth - uProjectionParams.x );
}

float getGBufferLinearDepth( vec2 uv )
{
	vec4 gbufferDepthSample = texture( uTexDepth, uv );
	return getLinearDepth( gbufferDepthSample.r );
}

vec3 getGBufferPosition( vec2 uv )
{
    vec4 posProj = vec4( ( uv.x - 0.5 ) * 2.0, ( uv.y - 0.5 ) * 2.0, 0.0, 1.0 );
    vec3 viewRay = ( uProjectionMatrixInverse * posProj ).xyz;
    return viewRay * getGBufferLinearDepth( uv );
}

// http://www.iquilezles.org/www/articles/fog/fog.htm
vec3 applyFog( in vec3 sceneColor, in vec3 fogColor, in float dist )
{
	float fog = exp( - fogDensity * dist * dist );
	return mix( fogColor, sceneColor, fog );
}


vec4 calcBlurredColor( vec2 coord )
{
	 const int numSamples = 40;
	 vec2 vel = texture( uTexVelocity, coord ).xy;

	 // TODO: this looks good around 20 for the objects in the scene, but that looks horrible for the camera motion blur
	 vel *= 1.5;

	 vec4 col = texture( uTexColor, coord );
	 for( int i = 1; i < numSamples; i++ ) {
	 	vec2 offset = vel * ( float(i) / float( numSamples - 1 ) - 0.5 );
	 	col += texture( uTexColor, coord + offset );
	 }
	 col /= float( numSamples );

	 return col;
}

void main()
{
	vec2 coord = vTexCoord;

	// FIXME: why is there nothing in the blue channel?
	// .. ok now it's working :/
	#if 0
		oFragColor = texture( uTexColor, coord );
		// oFragColor.b = 1.0; // this has no effect either
		// oFragColor.a = 1.0;
		return;
	#endif

	float dist = length( getGBufferPosition( coord ) );

	//float distAdjusted = dist - distStart;
	float distAdjusted = dist;

	vec4 col = vec4( 0 );

	col = texture( uTexColor, coord );

	// vec3 fogColor = texture( uTexBackground, coord ).rgb;

#if 0
	if( uFogEnabled )
		col.rgb = applyFog( col.rgb, fogColor, distAdjusted );
#endif

	// if( dist > distStart && dist < ( distStart + distRange ) ) {
	// 	col.r += 0.6;
	// }

	// col -= vec4( 0, 0.5, 0.5, 0.0 );

#if defined( BLOOM_ENABLED )
	vec4 glow = texture( uTexGlow, coord );
	// col.a += glow.a;
	col += glow;
#endif

	// tone mapping (disabled if uExposure < 0 )
	if( uExposure > 0 ) {
   		// col.rgb = toneMap( col.rgb, uExposure );
	}

#if defined( GAMMA_ENABLED )
    col.rgb = pow( clamp( col.rgb, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) );
#endif

   	// FXAA requires luma to be written to the alpha channel
   	const vec3 luminance = vec3( 0.299, 0.587, 0.114 );
#if AA_TYPE == AA_FXAA || AA_TYPE == AA_SMAA
   	float luma = dot( col.rgb, luminance );
   	// luma = pow( luma, 1.0 / 2.2 ); // note from FxaaPixelShader() docs: "{___a} = luma in perceptual color space (not linear)"
   	col.a = luma;
#endif

	oFragColor = col;
}
