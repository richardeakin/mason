/*
 Copyright (c) 2014-15, Richard Eakin - All rights reserved.

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

#include "mason/FileWatcher.h"

#include "cinder/app/App.h"

using namespace ci;
using namespace std;

namespace mason {

namespace {

fs::path findFullFilePath( const fs::path &filePath )
{
	if( filePath.empty() )
		throw FileWatcherException( "empty path" );

	if( filePath.is_absolute() && fs::exists( filePath ) )
		return filePath;

	auto resolvedAssetPath = app::getAssetPath( filePath );
	if( ! fs::exists( resolvedAssetPath ) )
		throw FileWatcherException( "could not resolve file path: " + filePath.string() );

	return resolvedAssetPath;
}
	
} // anonymous namespace

// ----------------------------------------------------------------------------------------------------
// Watch
// ----------------------------------------------------------------------------------------------------

void Watch::unwatch()
{
	mDiscarded = true;
}

// ----------------------------------------------------------------------------------------------------
// WatchSingle
// ----------------------------------------------------------------------------------------------------

WatchSingle::WatchSingle( const fs::path &filePath, const function<void ( const fs::path& )> &callback )
	: mCallback( callback )
{
	mFilePath = findFullFilePath( filePath );
	mTimeLastWrite = fs::last_write_time( mFilePath );
}

void WatchSingle::reload()
{
	mCallback( mFilePath );
}

bool WatchSingle::checkCurrent()
{
	if( ! fs::exists( mFilePath ) )
		return false;

	auto timeLastWrite = fs::last_write_time( mFilePath );
	if( mTimeLastWrite < timeLastWrite ) {
		mTimeLastWrite = timeLastWrite;
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------------------------------
// WatchMany
// ----------------------------------------------------------------------------------------------------

WatchMany::WatchMany( const vector<fs::path> &filePaths, const function<void ( const std::vector<fs::path>& )> &callback )
	: mCallback( callback )
{
	mFilePaths.reserve( filePaths.size() );
	mTimeStamps.reserve( filePaths.size() );
	for( const auto &fp : filePaths ) {
		auto fullPath = findFullFilePath( fp );
		mFilePaths.push_back( fullPath );
		mTimeStamps.push_back( fs::last_write_time( fullPath ) );
	}
}

void WatchMany::reload()
{
	mCallback( mFilePaths );
}

bool WatchMany::checkCurrent()
{
	const size_t numFiles = mFilePaths.size();
	for( size_t i = 0; i < numFiles; i++ ) {
		const fs::path &fp = mFilePaths[i];
		if( fs::exists( fp ) ) {
			auto timeLastWrite = fs::last_write_time( fp );
			auto &currentTimeLastWrite = mTimeStamps[i];
			if( currentTimeLastWrite < timeLastWrite ) {
				currentTimeLastWrite = timeLastWrite;
				return false;
			}
		}
	}

	return true;
}

// ----------------------------------------------------------------------------------------------------
// FileWatcher
// ----------------------------------------------------------------------------------------------------

bool	sWatchingEnabled = true;

// static
FileWatcher* FileWatcher::instance()
{
	static FileWatcher sInstance;
	return &sInstance;
}

FileWatcher::FileWatcher()
{
	if( sWatchingEnabled )
		connectUpdate();
}

void FileWatcher::connectUpdate()
{
	if( app::App::get() && ! mUpdateConn.isConnected() )
		mUpdateConn = app::App::get()->getSignalUpdate().connect( bind( &FileWatcher::update, this ) );
}

// static
void FileWatcher::setWatchingEnabled( bool enable )
{
	sWatchingEnabled = enable;
	if( enable )
		instance()->connectUpdate();
	else
		instance()->mUpdateConn.disconnect();
}

// static
bool FileWatcher::isWatchingEnabled()
{
	return sWatchingEnabled;
}

// static
WatchRef FileWatcher::load( const fs::path &filePath, const function<void( const fs::path& )> &callback )
{
	auto asset = watch( filePath, callback );
	asset->reload();

	return asset;
}

// static
WatchRef FileWatcher::load( const vector<fs::path> &filePaths, const function<void ( const vector<fs::path>& )> &callback )
{
	auto asset = watch( filePaths, callback );
	asset->reload();

	return asset;
}

// static
WatchRef FileWatcher::watch( const fs::path &filePath, const function<void( const fs::path& )> &callback )
{
	WatchRef asset( new WatchSingle( filePath, callback ) );
	instance()->watch( asset );

	return asset;
}

// static
WatchRef FileWatcher::watch( const vector<fs::path> &filePaths, const function<void ( const vector<fs::path>& )> &callback )
{
	WatchRef asset( new WatchMany( filePaths, callback ) );
	instance()->watch( asset );

	return asset;
}

void FileWatcher::watch( const WatchRef &asset )
{
	lock_guard<recursive_mutex> lock( mMutex );

	mWatchList.push_back( asset );
}

void FileWatcher::update()
{
	// try-lock, if we fail to acquire the mutex then we skip this update
	unique_lock<recursive_mutex> lock( mMutex, std::try_to_lock );
	if( ! lock.owns_lock() )
		return;

	try {
		auto it = mWatchList.begin();
		while( it != mWatchList.end() ) {
			auto watch = *it;
			CI_ASSERT( watch );

			if( watch->isDiscarded() ) {
				it = mWatchList.erase( it );
				continue;
			}

			if( watch->isEnabled() && ! watch->checkCurrent() )
				watch->reload();

			++it;
		}
	}
	catch( fs::filesystem_error & ) {
		// some file probably got locked by the system. Do nothing this update frame, we'll check again next
	}
}

const size_t FileWatcher::getNumWatchedFiles() const
{
	size_t result = 0;
	for( const auto &w : mWatchList ) {
		auto many = dynamic_pointer_cast<WatchMany>( w );
		if( many )
			result += many->getNumFiles();
		else
			result += 1;
	}

	return result;
}

} // namespace mason
