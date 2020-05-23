#version 430

in vec3 vNormal;
in vec4	vColor;
in vec2 vTexCoord;

out vec4 oFragColor;

void main()
{
	vec3 sunDir = normalize( vec3( 0, 0, -1 ) );

    vec3 normal = normalize( - vNormal );
    float sunDiff = clamp( dot( normal, sunDir ), 0.0, 1.0 );
    float skyDiff = clamp( dot( normal, vec3( 0, 1, 0 ) ), 0.0, 1.0 );

	vec3 col = vColor.rgb;

    col *= sunDiff;

    // col = mix( col, vec3( 0.02, 0.05, 0.1 ) * 2.0, skyDiff );
    // col += vec3( 0.02, 0.05, 0.1 ) * skyDiff * 3;

	oFragColor = vec4( col, 1 );
}
