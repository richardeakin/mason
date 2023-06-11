#version 430
/**
  \file data-files/shader/MotionBlur/MotionBlur_neighborMinMax.pix

  Computes the largest-magnitude velocity and lowest speed of any point that could
  possibly overlap points in the tile with corner at gl_FragCoord.xy * maxBlurRadius.

  Invoked from MotionBlur::computeNeighborMax().

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2018, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#include <compatibility.glsl>

// This is actually the tileMinMax buffer, but the way that the GBuffer
// infrastructure works renames it to the SS_POSITION_CHANGE_buffer
// from which it was computed.
// uniform sampler2D   SS_POSITION_CHANGE_buffer;
uniform sampler2D uVelocityMap; // full-resolution velocity
#define SS_POSITION_CHANGE_buffer uVelocityMap

uniform vec4 SS_POSITION_CHANGE_readMultiplyFirst = vec4( 1 );
uniform vec4 SS_POSITION_CHANGE_readAddSecond	 = vec4( 0 );

uniform vec4 SS_POSITION_CHANGE_writeMultiplyFirst = vec4( 1 );
uniform vec4 SS_POSITION_CHANGE_writeAddSecond	 = vec4( 0 );

#define tileMinMax SS_POSITION_CHANGE_buffer

out float4 neighborMinMax;

// Only gather neighborhood velocity into tiles that could be affected by it.
// In the general case, only six of the eight neighbors contribute:
//
//  This tile can't possibly be affected by the center one
//     |
//     v
//    ____ ____ ____
//   |    | ///|/// |
//   |____|////|//__|
//   |    |////|/   |
//   |___/|////|____|
//   |  //|////|    | <--- This tile can't possibly be affected by the center one
//   |_///|///_|____|
//
void main()
{
	// Vector with the largest magnitude
	float2 maxVelocity = float2( 0.0 );

	// Smallest magnitude
	float minSpeed = 1e6;

	// Squared magnitude of m
	float largestMagnitude2 = -1.0;

	int2 maxCoord = textureSize( tileMinMax, 0 ) - int2( 1 );

	int2 currentTile = int2( gl_FragCoord.xy );
	int2 offset;
	for( offset.y = -1; offset.y <= +1; ++offset.y ) {
		for( offset.x = -1; offset.x <= +1; ++offset.x ) {
			int2 neighborTile		= currentTile + offset;
			float3 vminmax_neighbor = texelFetch( tileMinMax, clamp( neighborTile, int2( 0 ), maxCoord ), 0 ).xyz * SS_POSITION_CHANGE_readMultiplyFirst.xyz + SS_POSITION_CHANGE_readAddSecond.xyz;

			float2 vmax_neighbor = vminmax_neighbor.xy;
			float smin_neighbor  = vminmax_neighbor.z;

			// Min speed is computed for the entire 3x3 neighborhood,
			// regardless of direction of propagation. That is because there
			// is no natural direction when vmax_neighbor = (0, 0).
			minSpeed = min( minSpeed, smin_neighbor );

			// Magnitude squared
			float magnitude2_neighbor = dot( vmax_neighbor, vmax_neighbor );

			if( magnitude2_neighbor > largestMagnitude2 ) {
				// Non-unit
				float2 directionOfVelocity = vmax_neighbor;

				// Manhattan distance to the tiles, which is used for
				// differentiating corners versus middle blocks
				float displacement = abs( offset.x ) + abs( offset.y );

				// Relative sign on each axis of the offset compared
				// to the velocity for that tile.  In order for a tile
				// to affect the center tile, it must have a
				// neighborhood velocity in which x and y both have
				// identical or both have opposite signs relative to
				// offset. If the offset coordinate is zero then
				// velocity is irrelevant.
				float2 point = sign( offset * directionOfVelocity );

				float distance = ( point.x + point.y );

				// Here's an example of the logic for this code.
				// In this diagram, the upper-left tile has offset = (-1, -1).
				// V1 is velocity = (1, -2). point in this case = (-1, 1), and therefore distance = 0,
				// so the upper-left tile does not affect the center.
				//
				// Now, look at another case. V2 = (-1, -2). point = (1, 1), so distance = 2 and the tile does
				// affect the center.
				//
				// V2(-1,-2)  V1(1, -2)
				//		  \	   /
				//         \  /
				//          \/___ ____ ____
				//	(-1, -1)|    |    |    |
				//			|____|____|____|
				//			|    |    |    |
				//			|____|____|____|
				//			|    |    |    |
				//			|____|____|____|
				//

				if( abs( distance ) == displacement ) {
					// This is the new largest PSF in the neighborhood
					maxVelocity		  = vmax_neighbor;
					largestMagnitude2 = magnitude2_neighbor;
				} // larger
			} // larger
		} // x
	} // y

	neighborMinMax.rg = maxVelocity * SS_POSITION_CHANGE_writeMultiplyFirst.xy + SS_POSITION_CHANGE_writeAddSecond.xy;
	neighborMinMax.b  = minSpeed * SS_POSITION_CHANGE_writeMultiplyFirst.z + SS_POSITION_CHANGE_writeAddSecond.z;
	// neighborMinMax.a = 1.0;
}
