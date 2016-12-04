////////////////////////////////////////////////////
// Cellular noise ("Worley noise") in 2D in GLSL, simplified version.
// Copyright (c) Stefan Gustavson 2011-04-19. All rights reserved.
// This code is released under the conditions of the MIT license.
// See LICENSE file for details.
// 
// minor edits by Richard Eakin

vec3 permute(vec3 x)
{
  return mod((34.0 * x + 1.0) * x, 289.0);
}

// Cellular noise, returning F1 and F2 in a vec2.
// Standard 3x3 search window for good F1 and F2 values
// - jitter was #define'd as 1 in original implementation. Less gives more regular pattern.
vec2 cellular( vec2 P, float jitter )
{
	const float K = 0.142857142857; // 1/7
	const float Ko = 0.428571428571; // 3/7
	vec2 Pi = mod(floor(P), 289.0);
 	vec2 Pf = fract(P);
	vec3 oi = vec3(-1.0, 0.0, 1.0);
	vec3 of = vec3(-0.5, 0.5, 1.5);
	vec3 px = permute(Pi.x + oi);
	vec3 p = permute(px.x + Pi.y + oi); // p11, p12, p13
	vec3 ox = fract(p*K) - Ko;
	vec3 oy = mod(floor(p*K),7.0)*K - Ko;
	vec3 dx = Pf.x + 0.5 + jitter*ox;
	vec3 dy = Pf.y - of + jitter*oy;
	vec3 d1 = dx * dx + dy * dy; // d11, d12 and d13, squared
	p = permute(px.y + Pi.y + oi); // p21, p22, p23
	ox = fract(p*K) - Ko;
	oy = mod(floor(p*K),7.0)*K - Ko;
	dx = Pf.x - 0.5 + jitter*ox;
	dy = Pf.y - of + jitter*oy;
	vec3 d2 = dx * dx + dy * dy; // d21, d22 and d23, squared
	p = permute(px.z + Pi.y + oi); // p31, p32, p33
	ox = fract(p*K) - Ko;
	oy = mod(floor(p*K),7.0)*K - Ko;
	dx = Pf.x - 1.5 + jitter*ox;
	dy = Pf.y - of + jitter*oy;
	vec3 d3 = dx * dx + dy * dy; // d31, d32 and d33, squared
	// Sort out the two smallest distances (F1, F2)
	vec3 d1a = min(d1, d2);
	d2 = max(d1, d2); // Swap to keep candidates for F2
	d2 = min(d2, d3); // neither F1 nor F2 are now in d3
	d1 = min(d1a, d2); // F1 is now in d1
	d2 = max(d1a, d2); // Swap to keep candidates for F2
	d1.xy = (d1.x < d1.y) ? d1.xy : d1.yx; // Swap if smaller
	d1.xz = (d1.x < d1.z) ? d1.xz : d1.zx; // F1 is in d1.x
	d1.yz = min(d1.yz, d2.yz); // F2 is now not in d2.yz
	d1.y = min(d1.y, d1.z); // nor in  d1.z
	d1.y = min(d1.y, d2.x); // F2 is in d1.y, we're done.
	return sqrt(d1.xy);
}

////////////////////////////////////////////////////

float hash( float n )
{
    return fract(sin(n)*43758.5453);
}

float noise( in vec2 x )
{	
    vec2 p = floor(x);
    vec2 f = fract(x);
    f = f*f*(3.0-2.0*f);
    float n = p.x + p.y*57.0;
    float res = mix(mix( hash(n+  0.0), hash(n + 1.0),f.x), mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y);
    return res;
}

// from: https://www.shadertoy.com/view/XslGRr
float noise3D( in vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);

    f = f*f*(3.0-2.0*f);
    float n = p.x + p.y*57.0 + 113.0*p.z;
    return mix(mix(mix( hash(n+  0.0), hash(n+  1.0),f.x),
                   mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
               mix(mix( hash(n+113.0), hash(n+114.0),f.x),
                   mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
}

float random( vec2 s )
{
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt= dot( s.xy, vec2( a, b ) );
    highp float sn= mod( dt, 3.14 );
    return fract(sin(sn) * c);
}

float random( float s )
{
	return random( vec2( s ) );
}

float random( vec2 s, float min, float max )
{
	return min + random( s ) * ( max - min );
}

float random( float s, float min, float max )
{
	return min + random( s ) * ( max - min );
}

float fbm( vec2 p )
{
	const mat2 m = mat2( 0.80,  0.60, -0.60,  0.80 );

	float f = 0;
	f += 0.50000 * noise( p ); p *= m * 2.02;
	f += 0.25000 * noise( p ); p *= m * 2.03;
	f += 0.12500 * noise( p ); p *= m * 2.01;
	f += 0.06250 * noise( p ); p *= m * 2.04;
	f += 0.03125 * noise( p );
    f /= 0.984375;

	return f;
}

float fbm4( vec2 p )
{
	const mat2 m = mat2( 0.8, 0.6, -0.6, 0.8 );
	
	float f = 0;
	f += 0.50000 * noise( p ); p *= m * 2.02;
	f += 0.25000 * noise( p ); p *= m * 2.03;
	f += 0.12500 * noise( p ); p *= m;
    f /= 0.875;

	return f;
}

// some mapping utils

float impulse( float k, float x )
{
    float h = k * x;
    return h * exp( 1.0 - h );
}

float range( float val, float inMin, float inMax, float outMin, float outMax )
{
	return outMin + (outMax - outMin) * ( (val - inMin) / (inMax - inMin) );
}
