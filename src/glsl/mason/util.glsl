
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

float random( float s, vec2 minMaxRange )
{
	return minMaxRange[0] + random( s ) * ( minMaxRange[1] - minMaxRange[0] );
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
