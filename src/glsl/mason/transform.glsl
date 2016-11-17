#ifndef M_PI
    #define M_PI 3.1415926535897932384626433832795
#endif

// http://www.neilmendoza.com/glsl-rotation-about-an-arbitrary-axis/
mat4 rotate( vec3 axis, float angle )
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4( oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                 oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                 oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                 0.0,                                0.0,                                0.0,                                1.0 );
}

mat3 quatToMat3( in vec4 q )
{
    mat3 result = mat3( 1 );
    float qxx = q.x * q.x;
    float qyy = q.y * q.y;
    float qzz = q.z * q.z;
    float qxz = q.x * q.z;
    float qxy = q.x * q.y;
    float qyz = q.y * q.z;
    float qwx = q.w * q.x;
    float qwy = q.w * q.y;
    float qwz = q.w * q.z;

    result[0][0] = 1 - 2 * ( qyy +  qzz );
    result[0][1] = 2 * ( qxy + qwz );
    result[0][2] = 2 * ( qxz - qwy );

    result[1][0] = 2 * ( qxy - qwz );
    result[1][1] = 1 - 2 * ( qxx +  qzz );
    result[1][2] = 2 * ( qyz + qwx );

    result[2][0] = 2 * ( qxz + qwy );
    result[2][1] = 2 * ( qyz - qwx );
    result[2][2] = 1 - 2 * ( qxx +  qyy );

    return result;
}

//! multiply a quat by a quat
vec4 quatMul( vec4 x, vec4 y )
{
    vec4 result;
    vec4 p = x;
    vec4 q = y;

    result.w = p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
    result.x = p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
    result.y = p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
    result.z = p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;
    return result;
}

//! multiply a quat by a vec3
vec3 quatMul( vec4 q, vec3 v )
{
    vec3 quatVector = vec3( q.x, q.y, q.z );
    vec3 uv = cross( quatVector, v );
    vec3 uuv = cross( quatVector, uv );

    return v + ( ( uv * q.w ) + uuv ) * 2;
}

//! returns a quat representing the angle \a angle and axis \a v
vec4 angleAxis( float angle, vec3 v )
{
    vec4 result;

    float a = angle;
    float s = sin( a * 0.5 );

    result.w = cos( a * 0.5 );
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    return result;
}

// same as glm::rotation() that returns a quat
vec4 rotation( vec3 orig, vec3 dest	)
{
    const float epsilon = 0.001;

    float cosTheta = dot( orig, dest );
    vec3 rotationAxis;

    if( cosTheta < -1.0 + epsilon ) {
        // special case when vectors in opposite directions :
        // there is no "ideal" rotation axis
        // So guess one; any will do as long as it's perpendicular to start
        // This implementation favors a rotation around the Up axis (Y),
        // since it's often what you want to do.
        rotationAxis = cross( vec3( 0, 0, 1 ), orig );
        if( dot( rotationAxis, rotationAxis ) < epsilon ) // bad luck, they were parallel, try again!
            rotationAxis = cross( vec3( 1, 0, 0 ), orig );

        rotationAxis = normalize( rotationAxis );
        return angleAxis( M_PI, rotationAxis );
    }


    // Implementation from Stan Melax's Game Programming Gems 1 article
    rotationAxis = cross( orig, dest );

    float s = sqrt( ( 1.0 + cosTheta ) * 2.0 );
    float invs = 1.0 / s;

    return vec4(
        s * 0.5,
        rotationAxis.x * invs,
        rotationAxis.y * invs,
        rotationAxis.z * invs
    );
}
