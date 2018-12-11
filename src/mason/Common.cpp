/*
 Copyright (c) 2014, Richard Eakin - All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided
 that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 the following disclaimer in the documentation and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "mason/Common.h"

#include "cinder/app/App.h"
#include "cinder/CinderAssert.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include "mason/Assets.h"
#include "mason/Hud.h"


using namespace ci;
using namespace std;

namespace mason {

namespace {

Rand sRand;

} // anonymous namespace

const fs::path& getRepoRootPath()
{
	static fs::path	sRepoRootPath;

	if( sRepoRootPath.empty() ) {
		const size_t maxDirectoryTraversals = 30;

		size_t parentCount = 0;
		for( auto currentPath = app::getAppPath(); currentPath.has_parent_path(); currentPath = currentPath.parent_path() ) {
			if( ++parentCount > maxDirectoryTraversals )
				break;

			const fs::path currentGitPath = currentPath / ".git";
			if( fs::exists( currentGitPath ) ) {
				sRepoRootPath = currentPath;
				break;
			}
		}

		CI_ASSERT( ! sRepoRootPath.empty() );
	}

	return sRepoRootPath;
}

// http://stackoverflow.com/a/12797413/506584
fs::path normalizePath( const fs::path &path )
{
	fs::path absPath = fs::absolute( path );
	fs::path::iterator it = absPath.begin();
	fs::path result = *it++;

	// Get canonical version of the existing part
	for( ; fs::exists( result / *it ) && it != absPath.end(); ++it ) {
		result /= *it;
	}
	result = canonical( result );

	// For the rest remove ".." and "." in a path with no symlinks
	for( ; it != absPath.end(); ++it ) {
		// Just move back on ../
		if( *it == ".." ) {
			result = result.parent_path();
		}
		// Ignore "."
		else if( *it != "." ) {
			// Just cat other path entries
			result /= *it;
		}
	}

	// Remove leading forward slash, if it exists
	if( !result.empty() && result.string().front() == '/' ) {
		string tmp = result.string();
		result = tmp.erase( 0, 1 );
	}

	return result;
}

// TODO: remove this and use filesystem::relative() when it is available on windows
std::string stripBasePath( const ci::fs::path &fullPath, const ci::fs::path &basePath )
{
	auto fullString = fullPath.generic_string();
	auto baseString = basePath.generic_string();
	auto remainderPos = fullString.find( baseString );

	if( remainderPos == string::npos )
		return fullString;

	return fullString.substr( baseString.size() + 1 );
}

ci::Timeline* timeline()
{
	return &app::timeline();
}

double currentTime()
{
	return app::getElapsedSeconds();
}

uint64_t currentFrame()
{
	return app::getElapsedFrames();
}

void notifyResourceReloaded()
{
	NotificationCenter::post( NOTIFY_RESOURCE_RELOADED );
}

void initialize( const fs::path &masonRootDir )
{
	initRand();
	log::makeOrGetLogger<ma::LoggerNotification>();

	if( ! masonRootDir.empty() ) {
		fs::path glslDir = masonRootDir / "src/glsl";
		if( fs::exists( glslDir ) && fs::is_directory( glslDir ) ) {
			ma::assets()->getShaderPreprocessor()->addSearchDirectory( glslDir );
		}
		else {
			CI_LOG_W( "not a directory: " << glslDir << ", is masonRootDir correct (" << masonRootDir << ")?" );
		}
	}
}


void initRand( bool randomizeSeed, uint32_t seed )
{
	if( randomizeSeed ) {
		auto now = std::chrono::system_clock::now().time_since_epoch();
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>( now ).count();
		sRand.seed( (int32_t)seconds );
	}
	else
		sRand.seed( seed );
}

Rand* rand()
{
	return &sRand;
}

vec3 randVec3( const AxisAlignedBox &box )
{
	return vec3(
		ma::rand()->nextFloat( box.getMin().x, box.getMax().x ),
		ma::rand()->nextFloat( box.getMin().y, box.getMax().y ),
		ma::rand()->nextFloat( box.getMin().z, box.getMax().z )
	);
}

ci::Color randColor()
{
	return Color( ma::rand()->nextFloat(), ma::rand()->nextFloat(), ma::rand()->nextFloat() );
}

bool epsilonEqual( const ci::Rectf &r1, const ci::Rectf &r2, float epsilon )
{
	return ( fabsf( r1.x1 - r2.x1 ) < epsilon ) && ( fabsf( r1.y1 - r2.y1 ) < epsilon ) && ( fabsf( r1.x2 - r2.x2 ) < epsilon ) && ( fabsf( r1.y2 - r2.y2 ) < epsilon );
}

string stackTraceAsString( size_t startingFrame, size_t count, bool skipPlatformFrames )
{
	if( skipPlatformFrames ) {
#if defined( CINDER_MSW )
		startingFrame += 4;
#elif defined( CINDER_COCOA )
		startingFrame += 3;
#else
		// unknown platform, use full stack trace;
#endif
	}

	auto st = stackTrace();
	if( st.empty() )
		return "(unknown)";

	size_t numFramesToRemove = min( startingFrame, st.size() );
	st.erase( st.begin(), st.begin() + numFramesToRemove );

	if( count == 0 )
		count = st.size();

	string result;
	for( size_t i = 0; i < count; i++ )
		result += "[" + to_string( i ) + "] " + st[i] + "\n";

	return result;
}

int nextOdd( int n )
{
	return n + 1 - ( n & 1 );
}

} // namespace mason
