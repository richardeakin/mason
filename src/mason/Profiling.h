#pragma once

#if ! defined( MA_PROFILING )
#define MA_PROFILING 1
#endif

#if ! defined( CI_PROFILING )
#define CI_PROFILING 1
#endif
#include "Profiler.h"

#include "cinder/gl/gl.h"

namespace mason {

class Marker {
public:
	static void swap() { id = 0; }
	static void push( const char *message )
	{
		glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, id++, -1, message );
	}
	static void pop()
	{
		glPopDebugGroup();
	}

private:
	Marker() = default;

	__declspec( thread ) static int id;
};

class ScopedMarker {
public:
	explicit ScopedMarker( const char *message ) { Marker::push( message ); }
	explicit ScopedMarker( std::string message ) { Marker::push( message.c_str() ); }
	~ScopedMarker() { Marker::pop(); }
};

} // namespace mason

#if MA_PROFILING
#define MA_MARKER( name )	mason::ScopedMarker __ma_marker{ name }
#define MA_PROFILE( name )  CI_PROFILE( name ); MA_MARKER( name )
#else
#define MA_MARKER( name )
#define MA_PROFILE( name )
#endif
