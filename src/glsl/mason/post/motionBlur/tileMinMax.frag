#version 430
/**
  \file data-files/shader/MotionBlur/MotionBlur_tileMinMax.pix

  Computes the largest-magnitude velocity in the tile with corner at gl_FragCoord.xy * maxBlurRadius.
  Invoked from MotionBlur::computeTileMax().

  This is run twice, once in each dimension.  It transposes the output from the input, so it doesn't need
  to know which pass number it is in.

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2018, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/

// #expect INPUT_HAS_MIN_SPEED "1 or 0, indicating if the input has min speed values"
#if ! defined( INPUT_HAS_MIN_SPEED )
  #error "expects INPUT_HAS_MIN_SPEED to be defined as either 1 or 0, indicating if the input has min speed values"
#endif

#include <compatibility.glsl>

// uniform sampler2D   SS_POSITION_CHANGE_buffer;
uniform sampler2D uVelocityMap; // full-resolution velocity
#define SS_POSITION_CHANGE_buffer uVelocityMap

// uniform vec4 SS_POSITION_CHANGE_readMultiplyFirst = vec4( 1 );
// uniform vec4 SS_POSITION_CHANGE_readAddSecond	 = vec4( 0 );

// uniform vec4 SS_POSITION_CHANGE_writeMultiplyFirst = vec4( 1 );
// uniform vec4 SS_POSITION_CHANGE_writeAddSecond	 = vec4( 0 );

/** Shift input pixel coordinates by this amount to compensate for the guard band */
uniform vec2 inputShift;

// Expects macro maxBlurRadius; // (rte) default = 43
uniform int maxBlurRadius;

out float4 tileMinMax; // TODO: should be a float3 like others?

void main()
{
	float2 m				= float2( 0.0 );
	float largestMagnitude2 = 0.0;
	float minSpeed			= 1e6;

	// Round down to the tile corner.  Note that we're only filtering in the x direction of the source,
	// so the y dimension is unchanged.
	//
	// Note the transpose relative to the input since the next pass will
	// transpose back for a gather in the other dimension
	int tileCornerX = int( gl_FragCoord.y ) * maxBlurRadius + int( inputShift.y );
	int tileRowY	= int( gl_FragCoord.x + inputShift.x );

	// This is relative to the input
  vec2 bufferSize = vec2( textureSize( SS_POSITION_CHANGE_buffer, 0 ) );
	int maxCoordX = textureSize( SS_POSITION_CHANGE_buffer, 0 ).x - 1 - int( inputShift.x );

	for( int offset = 0; offset < maxBlurRadius; ++offset ) {
		// Point at which we're gathering from
		int2 G = int2( clamp( tileCornerX + offset, inputShift.x, maxCoordX ), tileRowY );

		// Velocity at this point/tile
		float3 temp = texelFetch( SS_POSITION_CHANGE_buffer, G, 0 ).xyz;
		
    //temp.xy *= bufferSize; // convert to pixels (velocity map is in unit [-1:1] space)

    float2 v_G  = temp.xy;
		float speed = temp.z;

		// Magnitude squared
		float thisMagnitude2 = dot( v_G, v_G );
#if ! INPUT_HAS_MIN_SPEED
		// Store squared; we'll compute the square root below to avoid computing
		// that sqrt at every pixel in the gather
		speed = thisMagnitude2;
// On the 2nd pass, we have a valid speed in temp.z
#endif

		minSpeed = min( speed, minSpeed );

		if( thisMagnitude2 > largestMagnitude2 ) {
			// This is the new largest PSF
			m	= v_G;
			largestMagnitude2 = thisMagnitude2;
		}
	}

#if ! INPUT_HAS_MIN_SPEED
	minSpeed = sqrt( minSpeed );
#endif

	tileMinMax = vec4( m.x, m.y, minSpeed, 1 );
}
