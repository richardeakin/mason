#version 430

in vec2	vTexCoord;

uniform float uTime;
uniform vec4  uDevParams;
uniform vec3  uCamPos;
uniform vec3  uCamDir;
uniform vec2  uResolution;

out vec4 oFragColor;

struct Ray {
    vec3 origin;
    vec3 dir;
};

// world -> view space
mat3 calcLookAtMat3( in vec3 ro, in vec3 target, in float roll )
{
    vec3 ww = normalize( target - ro );
    vec3 uu = normalize( cross( ww, vec3( sin( roll ), cos( roll ), 0.0 ) ) );
    vec3 vv = normalize( cross( uu, ww ) );

    return mat3( uu, vv, ww );
}

//! Returns a Ray for pixel position p
Ray calcViewRay( in vec2 p, in vec3 camPos, in vec3 camDir, in float camRoll, in float lensLength )
{
    vec3 camTarget = camPos + camDir;
    mat3 viewMatrix = calcLookAtMat3( camPos, camTarget, camRoll );

    return Ray( camPos, normalize( viewMatrix * vec3( p.xy, lensLength ) ) );
}

void main()
{
	float camLensLength = 2.0;
	float camRoll = 0.0;
	vec2 p = ( -uResolution.xy + 2.0 * gl_FragCoord.xy ) / uResolution.y;
	Ray ray = calcViewRay( p, uCamPos, uCamDir, camRoll, camLensLength );

	vec3 col = vec3( 0.1, 0.12, 0.18 ) * 0.7;
    col += ray.dir.y * 0.15;

	oFragColor = vec4( col, 1 );
}