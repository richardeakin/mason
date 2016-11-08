#version 410

#include "transform.glsl"

// TODO:
// goal #2: orientate triangles by velocity

uniform mat4    ciModelViewProjection;

layout( points ) in;
layout( triangle_strip, max_vertices = 3 ) out;

in vec4[] vPositionWorld;
in vec4[] vMovement;

vec4 pos = vPositionWorld[0];
vec3 dir = vMovement[0].xyz;

vec4 orientationQuat = rotation( vec3( 0, 0, -1 ), normalize( dir ) );

void emitVert( vec3 vert )
{
        vert = quatMul( orientationQuat, vert );

        // TODO NEXT: probably need a line like this:
//		gl::rotate( angleAxis( float( M_PI / 2 ), vec3( 0, 0, 1 ) ) );

        gl_Position = ciModelViewProjection * ( pos + vec4( vert, 0 ) );
        EmitVertex();
}

void main()
{
    const float sz = 0.4;

    emitVert( vec3( -sz * 0.5, 0, 0 ) ); // left
    emitVert( vec3( sz * 0.5, 0, 0 ) );     // right
    emitVert( vec3( 0.0, sz, 0 ) ); // top / mid

    EndPrimitive();
}
