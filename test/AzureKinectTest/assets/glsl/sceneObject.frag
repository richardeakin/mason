#version 430

uniform int uJointConfidence = -1;
uniform vec3 uJointVelocity;

#define DEBUG_VELOCITIES 0

in vec3 vNormal;
in vec4	vColor;
in vec2 vTexCoord;

out vec4 oFragColor;

// --- analytically box-filtered checkerboard ---
// http://iquilezles.org/www/articles/checkerfiltering/checkerfiltering.htm
float checkersGrad( in vec2 p, in vec2 ddx, in vec2 ddy )
{
    // filter kernel
    vec2 w = max(abs(ddx), abs(ddy)) + 0.01;  
    // analytical integral (box filter)
    vec2 i = 2.0*(abs(fract((p-0.5*w)/2.0)-0.5)-abs(fract((p+0.5*w)/2.0)-0.5))/w;
    // xor pattern
    return 0.5 - 0.5*i.x*i.y;                  
}

void main()
{
	vec3 sunDir = normalize( vec3( 0, 0, -1 ) );

    vec3 normal = normalize( - vNormal );
    float sunDiff = clamp( dot( normal, sunDir ), 0.0, 1.0 );
    float skyDiff = clamp( dot( normal, vec3( 0, 1, 0 ) ), 0.0, 1.0 );

	vec3 col = vColor.rgb;

    // use uJointConfidence to color
    if( uJointConfidence >= 0 ) {
    	vec2 uv = vTexCoord * 14;
    	vec2 ddx = dFdx( uv ); 
		vec2 ddy = dFdy( uv ); 
		float check = checkersGrad( uv, ddx, ddy );
   		// float check = checkers( uv );
    	if( uJointConfidence == 0 ) {
    		// None: draw red checkers
    		col = mix( col, vec3( 1, 0, 0 ), check );
    	}
    	else if( uJointConfidence == 1 ) { // Low
    		// Low: draw black checkers
    		// col = clamp( col - check, vec3( 0 ), vec3( 1 ) );
    		col = mix( col, vec3( 0 ), check );
    	}
    	else if( uJointConfidence == 3 ) {
    		// High: draw white checkers (should not be getting these, according to sdk docs)
    		col = mix( col, vec3( 1 ), check );
    	}
    }

#if DEBUG_VELOCITIES
    col = vec3( 0.08 );
    vec3 vel = uJointVelocity;
    vel *= 0.02;
    if( vel.x > 0.0 )
        col.r = vel.x;
    else
        col.g = - vel.x;

    if( vel.y > 0 )
        col.b = vel.y;
    else {
        // -y: yellow
        col.r += vel.y * -0.5;
        col.g += vel.y * -0.5;
    }
#endif


    col *= sunDiff;

    // col = mix( col, vec3( 0.02, 0.05, 0.1 ) * 2.0, skyDiff );
    // col += vec3( 0.02, 0.05, 0.1 ) * skyDiff * 3;

	oFragColor = vec4( col, 1 );
}
