// TODO: want to move this to mason or even Cinder-Dart, but it is here so that analyzer works
// - need to revisit packages

import 'package:vector_math/vector_math.dart';

const double M_PI = PI;

Vector2 vec2( [x, y] )
{
    if( x == null ) {
        assert( y == null );
        x = 0.0;
        y = 0.0;
    }
    else if( y == null ) {
        assert( x is num );
        x = x.toDouble();
        y = x;
    }
    else {
        x = x.toDouble();
        y = y.toDouble();
    }

    return new Vector2( x, y );
}

Vector3 vec3( [x, y, z] )
{
    if( x == null ) {
        assert( y == null && z == null );
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }
    else if( x is Vector2 ) {
        assert( y != null && z == null );
        z = y.toDouble();
        y = x.y;
        x = x.x;
    }
    else if( y == null ) {
        assert( x is num && z == null );
        x = x.toDouble();
        y = x;
        z = x;
    }
    else {
        assert( z is num );
        x = x.toDouble();
        y = y.toDouble();
        z = z.toDouble();
    }

    return new Vector3( x, y, z );
}

Quaternion quat( [w, x, y, z] )
{
    if( w == null ) {
        // quat()
        assert( x == null && y == null && z == null );
        return new Quaternion.identity();
    }
    else if( w is Vector3 ) {
        if( x is Vector3 ) {
            // quat( vec3 u, vec3 v )
            assert( y == null && z == null );
            Vector3 u = w;
            Vector3 v = x;
            Vector3 LocalW = cross( u, v );
            double Dot = dot( u, v );
            Quaternion result = new Quaternion( 1.0 + Dot, LocalW.x, LocalW.y, LocalW.z );
            result.normalize();
            return result;
        }
        else {
            // quat( vec3 eulerAngle )
            assert( y == null && z == null );

            Vector3 eulerAngle = w;
            Vector3 c = vec3( cos( eulerAngle.x * 0.5 ), cos( eulerAngle.y * 0.5 ), cos( eulerAngle.z * 0.5 ) );
            Vector3 s = vec3( sin( eulerAngle.x * 0.5 ), sin( eulerAngle.y * 0.5 ), sin( eulerAngle.z * 0.5 ) );

            w = c.x * c.y * c.z + s.x * s.y * s.z;
            x = s.x * c.y * c.z - c.x * s.y * s.z;
            y = c.x * s.y * c.z + s.x * c.y * s.z;
            z = c.x * c.y * s.z - s.x * s.y * c.z;
        }
    }
    else if( y == null && x is Vector3 ) {
        // quat( double, vec3 )
        assert( z == null );
        w = w.toDouble();
        var v = x;
        z = v.z;
        y = v.y;
        x = v.x;
    }
    else {
        // quat( double, double, double, double )
        assert( y is num && z is num );
        x = x.toDouble();
        y = y.toDouble();
        z = z.toDouble();
        w = w.toDouble();
    }

    return new Quaternion( x, y, z, w );
}

Quaternion angleAxis( num angle, Vector3 v ) => new Quaternion.axisAngle( v, angle.toDouble() );

Vector3 cross( Vector3 x, Vector3 y ) => x.cross( y );
double length( v ) => v.length;
double length2( v ) => v.length2;
dynamic normalize( v ) => v.normalized();

double dot( x, y )
{
    if( x is Quaternion && y is Quaternion ) {
        // Quaternion class doesn't provide a dot product, this one is from glm's compute_dot<tquat>()
        Vector4 tmp = new Vector4( x.x * y.x, x.y * y.y, x.z * y.z, x.w * y.w );
        return ( tmp.x + tmp.y ) + ( tmp.z + tmp.w );
    }
    else {
        return x.dot( y );
    }
}

Quaternion rotation( Vector3 start, Vector3 dest )
{
    start.normalize();
    dest.normalize();

    double cosTheta = dot( start, dest );
    Vector3 rotationAxis;

    if( cosTheta < -1 + 0.001 ) {
        // special case when vectors in opposite directions:
        // there is no "ideal" rotation axis
        // So guess one; any will do as long as it's perpendicular to start
        rotationAxis = cross( vec3( 0.0, 0.0, 1.0 ), start );
        if( length2( rotationAxis ) < 0.01 ) // bad luck, they were parallel, try again!
            rotationAxis = cross( vec3( 1.0, 0.0, 0.0 ), start );

        rotationAxis = normalize( rotationAxis );
        return angleAxis( 180.0, rotationAxis );
    }

    rotationAxis = cross( start, dest );

    double s = sqrt( ( 1.0 + cosTheta ) * 2.0 );
    double invs = 1.0 / s;

    return quat(
        s * 0.5,
        rotationAxis.x * invs,
        rotationAxis.y * invs,
        rotationAxis.z * invs
    );
}

Quaternion slerp( Quaternion x,	Quaternion y, double a )
{
    const double EPSILON = 10e-6;

    Quaternion z = y.clone();
    double cosTheta = dot( x, y );

    // If cosTheta < 0, the interpolation will take the long way around the sphere.
    // To fix this, one quat must be negated.
    if( cosTheta < 0.0 ) {
        z = -y;
        cosTheta = -cosTheta;
    }

    // Perform a linear interpolation when cosTheta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
    if( cosTheta > 1.0 - EPSILON ) {
        // Linear interpolation
        return quat(
            mix( x.w, z.w, a ),
            mix( x.x, z.x, a ),
            mix( x.y, z.y, a ),
            mix( x.z, z.z, a )
        );
    }
    else {
        // Essential Mathematics, page 467 (and glm::slerp<quat>())
        double angle = acos( cosTheta );
//      return ( sin( ( 1.0 - a ) * angle ) * x + sin( a * angle ) * z ) / sin( angle );

        var result = x.scaled( sin( ( 1.0 - a ) * angle ) ) + z.scaled( sin( a * angle ) );
        result.scale( 1.0 / sin( angle ) );
        return result;
    }
}

Vector3 eulerAngles( Quaternion x )
{
    return new Vector3( pitch( x ), yaw( x ), roll( x ) );
}

double yaw( Quaternion q )
{
    return asin( -2.0 * ( q.x * q.z - q.w * q.y ) );
}

double pitch( Quaternion q )
{
    return atan2( 2.0 * ( q.y * q.z + q.w * q.x ), q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z );
}

double roll( Quaternion q )
{
    return atan2( 2.0 * ( q.x * q.y + q.w * q.z ), q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z );
}

Quaternion lerp( Quaternion x, Quaternion y, double a )
{
    // Lerp is only defined in [0, 1]
    assert( a >= 0.0 );
    assert( a <= 1.0 );

//    return x * ( 1.0 - a ) + ( y * a );
    return x.scaled( 1.0 - a ) + y.scaled( a );
}

num range( num val, num inMin, num inMax, num outMin, num outMax )
{
    return outMin + ( outMax - outMin ) * ( ( val - inMin ) / ( inMax - inMin ) );
}

num randRange( var arg, [double max] )
{
    if( arg is Vector2 )
        return range( rand.nextDouble(), 0, 1, arg[0], arg[1] );
    else
        return range( rand.nextDouble(), 0, 1, arg, max );
}

double clamp( double x, double min, double max )
{
    return x.clamp( min, max );
}

Vector3 pow3( Vector3 v, double t )
{
    return new Vector3( pow( v.x, t ), pow( v.y, t ), pow( v.z, t ) );
}

double abs( double x )
{
    return x.abs();
}