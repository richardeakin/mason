#version 410

#include "util.glsl"
#include "hg_sdf.glsl"

uniform float ciElapsedSeconds;

uniform vec2	uResolution;

// TODO: test uniform with no default value
uniform bool    uFlagA = false;    // hud: "flag A"
uniform bool    uFlagB = true;     // hud: "flag B"
uniform float   uSliderA = 2.0;    // hud: "slider A", min = 0.2, max = 0.8
uniform vec2	uNBox2 = vec2( 5 ); // hud: "nbox2", step = 0.1
uniform vec3	uNBox3 = vec3( 0.1, 0.2, 0.3 ); // hud: "nbox3", min = 0, max = 1, step = 0.01
uniform vec4	uNBox4 = vec4( 10, 11, 12, 13 ); // hud: "nbox4", min = 5, max = 1000

in vec4 vPosition;
in vec2 vTexCoord0;

out vec4 oFragColor;

void main()
{
	vec2 fragCoord = gl_FragCoord.xy;
    vec2 p = (-uResolution.xy + 2.0 * fragCoord.xy) / uResolution.y;

    vec3 col = uNBox3;

//    vec2 x = p * uSliderA;
    vec2 x = p * uNBox2;

    if( uFlagA )
        col.gb += fract( x );
    else    
        col.rg += fract( x );

    float radius = dot( p, p );
    if( radius < uSliderA ) {
    	col.rg -= 0.4;
    }

    oFragColor = vec4( col, 1.0 );
}
