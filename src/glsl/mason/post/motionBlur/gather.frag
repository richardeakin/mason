#version 430

#include "mason/util.glsl"

#if ! defined( DEPTH_IN_COLOR_ALPHA_CHANNEL )
  #error "expects DEPTH_IN_COLOR_ALPHA_CHANNEL to be defined"
#endif

// Included by all of the blur shaders
#include "compatibility.glsl"

uniform sampler2D uColorMap; // texture we're blurring
uniform sampler2D uVelocityMap;
uniform sampler2D uNeighborMinMaxMap;

uniform float uVelocityScale = 1.0;

out float3 resultColor;

/**
  \file data-files/shader/MotionBlur/MotionBlur_gather.pix

  This is designed to read from a G3D-style velocity (optical flow) buffer.
  For performance, you could just write out velocity in the desired 
  format rather than adjusting it per texture fetch.

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2018, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/

//#expect maxBlurRadius "int > 0" // (rte): G3D default = 71, max = 141
// #define maxBlurRadius 43
uniform int maxBlurRadius;
//#expect numSamplesOdd           // (rte): G3D default = 27
uniform int numSamplesOdd;

// Set to 0 to make very thin objects appear correct, set to 1 to reduce noise but
// undersample single-pixel thick moving objects
#define SMOOTHER 1

/** Unprocessed differences between previous and current frame in screen space. Guard band. */
// uniform sampler2D   SS_POSITION_CHANGE_buffer;
#define SS_POSITION_CHANGE_buffer uVelocityMap

uniform vec4 SS_POSITION_CHANGE_readMultiplyFirst = vec4( 1 );
uniform vec4 SS_POSITION_CHANGE_readAddSecond	 = vec4( 0 );

/** Uses the same encoding as SS_POSITION_CHANGE but has three channels.  No guard band. */
// uniform sampler2D   neighborMinMax_buffer;
#define neighborMinMax_buffer uNeighborMinMaxMap

/** No guard band. */
// uniform sampler2D   colorBuffer;
#define colorBuffer uColorMap

/** Typical hyperbolic depth buffer: close values are greater than distant values. Guard band. */
// uniform sampler2D   depthBuffer;
uniform sampler2D uTexDepth;
#define depthBuffer uTexDepth

/** 32x32 tiled random numbers */
// uniform sampler2D   randomBuffer;

/** In fraction of frame duration */
// : G3D default = 0.75, max = 2.0
uniform float exposureTime = 0.75;

uniform ivec2 trimBandThickness; // (rte): G3D default = [64,64]

// TODO: remove this if no longer needed (think I added it for testing)
uniform float uSoftDepthExtent = 0.01; // ui: "z extent", min = 0.001, max = 0.5, step = 0.001

/** Amount on [-0.5, 0.5] to randomly perturb samples by at each pixel */
//float2 jitter;

/* Measured in pixels
   Make this smaller to better hide tile boundaries
   Make this bigger to get smoother blur (less noise) */
#define varianceThreshold 1.5

// Constant indicating locations wme we clamp against the minimum PSF, 1/2 pixel
const float HALF_PIX = 0.5;

/** Computes a pseudo-random number on [0, 1] from pixel position c. */
float hash( int2 c )
{
	// #   if numSamplesOdd <= 5
	// 	    // Use a simple checkerboard if you have very few samples; this gives too much ghosting
	//         // for many scenes, however
	//         return float(int(c.x + c.y) & 1) * 0.5 + 0.25;
	// #   else
	//         return texelFetch(randomBuffer, int2(c.x & 31, c.y & 31), 0).r;
	// #   endif
	return fract( sin( dot( c.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

/** Called from readAdjustedVelocity() and readAdjustedNeighborhoodVelocity() 

    (xy) is the velocity, z is the minimum blur radius in the tile for neighbor velocity
    and zero for expressive motion.
*/
float3 readAdjustedVelocity( int2 C, sampler2D sampler, out float r )
{
	// Raw screen-space movement
	float3 q = texelFetch( sampler, C, 0 ).xyz * SS_POSITION_CHANGE_readMultiplyFirst.xyz + SS_POSITION_CHANGE_readAddSecond.xyz;
    q *= uVelocityScale;

    //screenSize = textureSize( colorBuffer, 0 ).xy; // + trimBandThickness * 2 - int2( 1 );

	// return q;

	float lenq = length( q.xy );

	// Convert the velocity to be a radius instead of a diameter, and scale it by
	// the exposure time
	r = lenq * 0.5 * exposureTime;
	q.z *= 0.5 * exposureTime;
	bool rMuchLargerThanZero = ( r >= 0.01 );

	// Clamp to the usable distance
	r = clamp( r, HALF_PIX, float( maxBlurRadius ) ); // TODO: don't clamp this? it makes it so velocity scale > 1 doesn't do anything

	// If r is nearly zero, then we risk having a negligible value in lenq, so just return the original
	// vector, which can't be that long anyway if we entered this case
	if( rMuchLargerThanZero ) {
		// Adjust q's length based on the newly clamped radius
		q.xy *= ( r / lenq );
	}

	return q;
}

/** 
  V[C] in the paper.

  v = half-velocity vector 
  r = magnitude of v
*/
float3 readAdjustedVelocity( int2 C, out float r )
{
    // multiply through size (velocity map is in unit [-1:1] space)
    vec3 result = readAdjustedVelocity( C, SS_POSITION_CHANGE_buffer, r );
    //result.xy *= vec2( textureSize( SS_POSITION_CHANGE_buffer, 0 ) );
	return result;
}

/** NeighborMax[C] from the paper */
float3 readAdjustedNeighborhoodVelocity( int2 C, out float r )
{
	return readAdjustedVelocity( int2( C / float( maxBlurRadius ) ), neighborMinMax_buffer, r );
}

float cone( float dist, float r )
{
	return saturate( 1.0 - abs( dist ) / r );
}

float fastCone( float dist, float invR )
{
	return saturate( 1.0 - abs( dist ) * invR );
}

// A cone filter with maximum weight 1 at dist = 0 and min weight 0 at |v|=dist.
float cylinder( float dist, float r )
{
	//return 1.0 - smoothstep(r * 0.95, r * 1.05, abs(dist));

	// Alternative: (marginally faster on GeForce, comparable quality)
	return sign( r - abs( dist ) ) * 0.5 + 0.5;

	// The following gives nearly identical results and may be faster on some hardware,
	// but is slower on GeForce
	//    return (abs(dist) <= r) ? 1.0 : 0.0;
}

/** 0 if depth_A << depth_B, 1 if depth_A >> z_depth, fades between when they are close */
float softDepthCompare( float depth_A, float depth_B )
{
	// World space distance over which we are conservative about the classification
	// of "foreground" vs. "background".  Must be > 0.
	// Increase if slanted surfaces aren't blurring enough.
	// Decrease if the background is bleeding into the foreground.
	// Fairly insensitive
	// const float SOFT_DEPTH_EXTENT = 0.01;

	return saturate( 1.0 - ( depth_B - depth_A ) / uSoftDepthExtent );
}

// For linear Z values where more negative = farther away from camera
float softZCompare( float z_A, float z_B )
{
	// World space distance over which we are conservative about the classification
	// of "foreground" vs. "background".  Must be > 0.
	// Increase if slanted surfaces aren't blurring enough.
	// Decrease if the background is bleeding into the foreground.
	// Fairly insensitive
	const float SOFT_Z_EXTENT = 0.1;

	return saturate( 1.0 - ( z_A - z_B ) / SOFT_Z_EXTENT );
}

void main()
{
	// Size of the screen
	int2 SCREEN_MAX				 = textureSize( colorBuffer, 0 ).xy + trimBandThickness * 2 - int2( 1 );
	int2 NO_TRIM_BAND_SCREEN_MAX = textureSize( colorBuffer, 0 ).xy - int2( 1 );

	// Center pixel
	int2 me = int2( gl_FragCoord.xy );

	// A pseudo-random number on [-0.5, 0.5]
	float jitter = hash( me ) - 0.5;

    vec4 pixelColor = texelFetch( colorBuffer, me - trimBandThickness, 0 );
    resultColor = pixelColor.rgb;

	// resultColor = texelFetch( colorBuffer, me - trimBandThickness, 0 ).rgb;

#if DEPTH_IN_COLOR_ALPHA_CHANNEL
    float depth_center = pixelColor.a;
    depth_center = 1.0 - depth_center / 300.0; // TODO: this is ad-hoc for raymarcher, but this needs to be generalized.
    //resultColor = vec3( depth_center );
    //return;
#else
    // TODO: make sure this value is correct, it is appearing full white
    float depth_center = texelFetch( depthBuffer, me, 0 ).x;
#endif

    // FIXME: this region isn't getting blurred, don't yet know why
    // {
    //     int2 sizeNeighborMax = textureSize( neighborMinMax_buffer, 0 ).xy;
    //     float n = float( me.y ) / float( NO_TRIM_BAND_SCREEN_MAX.y );
    //     float blah = NO_TRIM_BAND_SCREEN_MAX.y / sizeNeighborMax.y;
    //     if( me.y > NO_TRIM_BAND_SCREEN_MAX.y - 66 ) {
    //         resultColor.r += 0.5;
    //         return;
    //     }       
    // }



	// Compute the maximum PSF in the neighborhood
	float r_neighborhood;

	float2 v_neighborhood;
	float rmin_neighborhood;
	{
		float3 temp		  = readAdjustedNeighborhoodVelocity( me - trimBandThickness, r_neighborhood );
		v_neighborhood	= temp.xy;
		rmin_neighborhood = temp.z;

		// resultColor = temp.xyz;
		// return;
	}

	if( r_neighborhood <= 0.505 ) {
		// there's no blurring at all in this pixel's neighborhood, since the maximum
		// velocity is less than one pixel.
		//resultColor.b += 0.5;
		return;
	}

    // if( r_neighborhood > 3 ) {
    //     resultColor = vec3( 0, 0, 1 );
    //     return;        
    // }

	// Compute PSF at me (this pixel)
	float radius_center;
	float2 velocity_center = readAdjustedVelocity( me, radius_center ).xy;

    // if( length( velocity_center ) > 0.001 ) {
    //     resultColor = vec3( 1, 0, 0 );
    //     return;
    // }

	// Above this pixel displacement, the center velocity overrides the neighborhood.
	// This ensures the overblurring occurs only on the interior of fast-moving objects
	// instead of in tiles outside of moving objects.
	const float centerOverrideThresholdPixels = 5;

	// Let w be a velocity direction (i.e., w is "omega", a unit vector in screen-space)
	// Let r be a half-velocity magnitude (i.e., a point-spread function radius)
	// If the center is moving very fast, sample along the center direction to avoid tile polution
	float2 w_neighborhood = normalize( ( radius_center >= centerOverrideThresholdPixels ) ? velocity_center : v_neighborhood );

	// Choose the direction at this pixel to be the same as w_neighborhood if this pixel is not itself moving.
	// Don't adjust the radious--doing so causes the background to blur out when it is static.
	float2 w_center = ( radius_center < varianceThreshold ) ? w_neighborhood : normalize( velocity_center );

	// Accumulated color; start with the center sample
	// Higher initial weight increases the ability of the background
	// to overcome the out-blurred part of moving objects
	float invRadius_center = 1.0 / radius_center;
	float totalCoverage	= ( float( numSamplesOdd ) / 40.0 ) * invRadius_center;
	resultColor *= totalCoverage;

	float radius_sample = r_neighborhood;

	// The following branch is coherent on tile boundaries. It gives about a 12% speedup
	// to motion blur due to camera motion. Ideally, the tile boundaries are also warp (8 pixel) boundaries
	// to avoid divergence.
	if( ( rmin_neighborhood >= r_neighborhood ) || ( ( rmin_neighborhood >= r_neighborhood * 0.65 ) && ( rmin_neighborhood >= 4 ) ) ) {

		// resultColor = vec3( 0, 0, 1 );
		// return;

        // Everything in this neighborhood is moving pretty fast; assume that
        // radius_sample == r_neighborhood and don't bother spending
        // bandwidth to actually read it per pixel
#define COMPUTE_RADIUS_SAMPLE()
		// The inner loop of MotionBlur_gather.pix
		// Sample along the largest PSF vector in the neighborhood
		for( int i = 0; i < numSamplesOdd; ++i ) {
            // The original algorithm ignores the center sample, but we include it because doing so
            // produces better results for thin objects at the expense of adding a slight amount of grain.
            // That is because the jitter will bounce this slightly off the actual center
#if SMOOTHER
			if( i == numSamplesOdd / 2 ) {
				continue;
			}
#endif

			// Signed step distance from X to Y.
			// Because cone(r_Y) = 0, we need this to never reach +/- r_neighborhood, even with jitter.
			// If this value is even slightly off then gaps and bands will appear in the blur.
			// This pattern is slightly different than the original paper.
			float t = clamp( 2.4 * ( float( i ) + 1.0 + jitter ) / ( numSamplesOdd + 1.0 ) - 1.2, -1, 1 );

			float dist = t * r_neighborhood;

			// Alternate between the neighborhood direction and this pixel's direction.
			// This significantly helps avoid tile boundary problems when other are
			// two large velocities in a tile. Favor the neighborhood velocity on the farthest
			// out taps (which also means that we get slightly more neighborhood taps, as we'd like)
            float2 sampling_direction = ( ( ( i & 1 ) == 1 ) ? w_center : w_neighborhood );
			float2 offset = dist * sampling_direction;

			// Point being considered; offset and round to the nearest pixel center.
			// Then, clamp to the screen bounds
			int2 other = clamp( int2( offset + gl_FragCoord.xy ), trimBandThickness, SCREEN_MAX );

            vec4 colorBufferValue = texelFetch( colorBuffer, clamp( other - trimBandThickness, ivec2( 0 ), NO_TRIM_BAND_SCREEN_MAX ), 0 );
            vec3 color_sample = colorBufferValue.rgb;
#if DEPTH_IN_COLOR_ALPHA_CHANNEL
            float depth_sample = colorBufferValue.a;
#else
            float depth_sample = texelFetch( depthBuffer, other, 0 ).x;
#endif

			// is other in the foreground or background of me?
			float inFront = softDepthCompare( depth_center, depth_sample );
			float inBack  = softDepthCompare( depth_sample, depth_center );

			// Relative contribution of sample to the center
			float coverage_sample = 0.0;

			// Blurry me, estimate background
			coverage_sample += inBack * fastCone( dist, invRadius_center );

			COMPUTE_RADIUS_SAMPLE();


			// Blurry other over any me
			coverage_sample += inFront * cone( dist, radius_sample );

			// Mutually blurry me and other (Optimized implementation)
			coverage_sample += cylinder( dist, min( radius_center, radius_sample ) ) * 2.0;

			//coverage_sample = saturate(coverage_sample * abs(dot(normalize(velocity_sample), sampling_direction)));
			//coverage_sample = saturate(dot(normalize(velocity_sample), sampling_direction));
			// Code from paper:
			// cylinder(dist, radius_center) * cylinder(dist, radius_sample) * 2.0;

			// Accumulate (with premultiplied coverage)
			resultColor += color_sample * coverage_sample;
			totalCoverage += coverage_sample;
		}
	}
	else {
        // resultColor = vec3( 0, 1, 0 );
        // return;

// Read true velocity at each pixel
#undef COMPUTE_RADIUS_SAMPLE

// The actual velocity_sample vector will be ignored by the code below,
// but the magnitude (radius_sample) of the blur is used.
#define COMPUTE_RADIUS_SAMPLE()	{ float2 velocity_sample = readAdjustedVelocity( other, radius_sample ).xy; }

		// int2 other = clamp(int2(vec2( 0 ) + gl_FragCoord.xy), trimBandThickness, SCREEN_MAX);
		// resultColor = vec3( readAdjustedVelocity(other, radius_sample).xy, 0 );
		// return;

		// The inner loop of MotionBlur_gather.pix
		// Sample along the largest PSF vector in the neighborhood
		for( int i = 0; i < numSamplesOdd; ++i ) {
            // The original algorithm ignores the center sample, but we include it because doing so
            // produces better results for thin objects at the expense of adding a slight amount of grain.
            // That is because the jitter will bounce this slightly off the actual center
#if SMOOTHER
			if( i == numSamplesOdd / 2 ) {
				continue;
			}
#endif

			// Signed step distance from X to Y.
			// Because cone(r_Y) = 0, we need this to never reach +/- r_neighborhood, even with jitter.
			// If this value is even slightly off then gaps and bands will appear in the blur.
			// This pattern is slightly different than the original paper.
			float t = clamp( 2.4 * ( float( i ) + 1.0 + jitter ) / ( numSamplesOdd + 1.0 ) - 1.2, -1, 1 );

			float dist = t * r_neighborhood;

            // Alternate between the neighborhood direction and this pixel's direction.
            // This significantly helps avoid tile boundary problems when other are
            // two large velocities in a tile. Favor the neighborhood velocity on the farthest
            // out taps (which also means that we get slightly more neighborhood taps, as we'd like)
			float2 sampling_direction = ( ( ( i & 1 ) == 1 ) ? w_center : w_neighborhood );
			float2 offset = dist * sampling_direction;

			// Point being considered; offset and round to the nearest pixel center.
			// Then, clamp to the screen bounds
			int2 other = clamp( int2( offset + gl_FragCoord.xy ), trimBandThickness, SCREEN_MAX );

			// float depth_sample = texelFetch( depthBuffer, other, 0 ).x;
            vec4 colorBufferValue = texelFetch( colorBuffer, clamp( other - trimBandThickness, ivec2( 0 ), NO_TRIM_BAND_SCREEN_MAX ), 0 );
            vec3 color_sample = colorBufferValue.rgb;
#if DEPTH_IN_COLOR_ALPHA_CHANNEL
            float depth_sample = colorBufferValue.a;
#else
            float depth_sample = texelFetch( depthBuffer, other, 0 ).x;
#endif

			// is other in the foreground or background of me?
			float inFront = softDepthCompare( depth_center, depth_sample );
			float inBack  = softDepthCompare( depth_sample, depth_center );

			// Relative contribution of sample to the center
			float coverage_sample = 0.0;

			// Blurry me, estimate background
			coverage_sample += inBack * fastCone( dist, invRadius_center );

			COMPUTE_RADIUS_SAMPLE();

			//float3 color_sample = texelFetch( colorBuffer, clamp( other - trimBandThickness, ivec2( 0 ), NO_TRIM_BAND_SCREEN_MAX ), 0 ).rgb;

			// Blurry other over any me
			coverage_sample += inFront * cone( dist, radius_sample );

			// Mutually blurry me and other (Optimized implementation)
			coverage_sample += cylinder( dist, min( radius_center, radius_sample ) ) * 2.0;

			// coverage_sample = saturate(coverage_sample * abs(dot(normalize(velocity_sample), sampling_direction)));
			// coverage_sample = saturate(dot(normalize(velocity_sample), sampling_direction));
			// Code from paper:
			// cylinder(dist, radius_center) * cylinder(dist, radius_sample) * 2.0;

			// Accumulate (with premultiplied coverage)
			resultColor += color_sample * coverage_sample;
			totalCoverage += coverage_sample;
		}
#undef COMPUTE_RADIUS_SAMPLE
	}

	// We never divide by zero because we always sample the pixel itself.
	resultColor /= totalCoverage;
}
