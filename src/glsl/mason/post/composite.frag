// Default fragment shader for PostProces compositing

#version 430

#include "mason/post/toneMap.glsl"

#define AA_NONE 0
#define AA_MSAA 1
#define AA_FXAA 2
#define AA_SMAA 3

#ifndef AA_TYPE
#define AA_TYPE AA_NONE // disabled
#endif

#if defined( VIGNETTE_ENABLED )
uniform float uVignetteIntensity = 60;
uniform float uVignetteExtent = 0.5;
uniform float uVignetteBlend = 0.5;
#endif

#if defined( BLOOM_ENABLED )
uniform float uGlowContrib = 0.5;
#endif

// for computing linear depth
uniform vec4 uProjectionParams; // x = cam near clip, y = cam far clip
uniform mat4 uProjectionMatrixInverse;

uniform float uExposure = 1.0;
uniform bool uFogEnabled = false;
uniform vec3 uFogColor = vec3( 0, 1, 1 ); // should match current scene's gl::clear().

uniform float uFogDensity = 0.002;
uniform float uFogDistStart = 0.4;
uniform float uFogDistScale = 10;

// Scene Attachments
uniform sampler2D uTexColor;
uniform sampler2D uTexDepth;
uniform sampler2D uTexGlow;
uniform sampler2D uTexVelocity;

in vec2	vTexCoord;
out vec4 oFragColor;

// http://web.archive.org/web/20130414004640/http://www.geeks3d.com/20091216/geexlab-how-to-visualize-the-depth-buffer-in-glsl/
float LinearizeDepth( vec2 uv, float znear, float zfar )
{
	float n = znear; // camera z near
	float f = zfar; // camera z far
	float z = texture2D( uTexDepth, uv ).x;
	return ( 2.0 * n ) / ( f + n - z * ( f - n ) );	
}

// http://www.iquilezles.org/www/articles/fog/fog.htm
vec3 applyFog( in vec3 sceneColor, in float density, in vec3 fogColor, in float dist )
{
	float fog = exp( - density * dist * dist );
	return mix( fogColor, sceneColor, fog );
}


void main()
{
	vec2 coord = vTexCoord;

	vec4 col = vec4( 0 );
	vec4 glow = vec4( 0 );

	col = texture( uTexColor, coord );

	if( uFogEnabled ) {
		// vec3 fogColor = texture( uTexBackground, coord ).rgb;
		vec3 fogColor = uFogColor;
		float depth = LinearizeDepth( coord, uProjectionParams.x, uProjectionParams.y );
		// depth = max( 0.0, depth * uFogDistScale - uFogDistStart );
		depth *= uFogDistScale;
		depth -= uFogDistStart;
		col.rgb = applyFog( col.rgb, uFogDensity, fogColor, depth );
	}


#if defined( BLOOM_ENABLED )
	glow = texture( uTexGlow, coord );
	col += glow * uGlowContrib;
#endif


	// tone mapping (disabled if uExposure < 0 )
	if( uExposure > 0 ) {
   		col.rgb = toneMap( col.rgb, uExposure );
	}

#if defined( GAMMA_ENABLED )
    col.rgb = pow( clamp( col.rgb, 0.0, 1.0 ), vec3( 1.0 / 2.2 ) );
#endif


#if defined( VIGNETTE_ENABLED )
    vec2 uv = 1.0 - abs( coord * 2.0 - 1.0 );
    float vignette = uv.x * uv.y * uVignetteIntensity;
    vignette = pow( vignette, uVignetteExtent ); // change pow for modifying the extend of the  vignette
    vignette = clamp( vignette, 0.0, 1.0 );
    // vignette -= noise.r;
    col.rgb *= mix( 1.0, vignette, uVignetteBlend );
    // col.rgb = vec3( uv, 0 );
    // col.rgb = vec3( vignette );
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
