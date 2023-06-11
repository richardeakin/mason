#ifndef MASON_UTIL_GLSL
#define MASON_UTIL_GLSL

#ifndef PI
#define PI 3.141592653589793
#endif

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

float random( float s, vec2 minMaxRange )
{
	return minMaxRange[0] + random( s ) * ( minMaxRange[1] - minMaxRange[0] );
}

vec3 random( vec3 s )
{
    return vec3( random( s.x + s.y ), random( s.x + s.z ), random( s.y + s.z ) );
}

bool epsilonEquals( float a, float b, float epsilon )
{
    return abs( a - b ) < epsilon;
}

bool epsilonEquals( float a, float b )
{
    return epsilonEquals( a, b, 0.001 );
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

vec3 range( vec3 val, vec3 inMin, vec3 inMax, vec3 outMin, vec3 outMax )
{
    return outMin + (outMax - outMin) * ( (val - inMin) / (inMax - inMin) );
}

vec3 range( vec3 val, float inMin, float inMax, float outMin, float outMax )
{
    return outMin + (outMax - outMin) * ( (val - inMin) / (inMax - inMin) );
}

#endif // MASON_UTIL_GLSL
