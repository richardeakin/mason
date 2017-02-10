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
#include "mason/Profiling.h"

#include "cinder/app/App.h"
#include "cinder/Log.h"

//#define LOG_UPDATE( stream )	CI_LOG_I( stream )
#define LOG_UPDATE( stream )	( (void)( 0 ) )

using namespace ci;
using namespace std;

namespace mason {

//! Base class for Watch types, which are returned from FileWatcher::load() and watch()
class Watch : public std::enable_shared_from_this<Watch>, private ci::Noncopyable {
  public:
	virtual ~Watch() = default;

	//! Checks if the asset file is up-to-date. Also may discard the Watch if there are no more connected slots.
	virtual void checkCurrent() = 0;
	//! Remove any watches for \a filePath. If it is the last file associated with this Watch, discard
	virtual void unwatch( const fs::path &filePath ) = 0;
	//! Emit the signal callback. 
	virtual void emitCallback() = 0;

	//! Marks the Watch as needing its callback to be emitted on the main thread.
	void setNeedsCallback( bool b )	{ mNeedsCallback = b; }
	//! Returns whether the Watch needs its callback emitted on the main thread.
	bool needsCallback() const		{ return mNeedsCallback; }
	//! Marks the Watch as discarded, will be destroyed the next update loop
	void markDiscarded()			{ mDiscarded = true; }
	//! Returns whether the Watch is discarded and should be destroyed.
	bool isDiscarded() const		{ return mDiscarded; }


  private:
	bool mDiscarded = false;
	bool mNeedsCallback = false;
};

//! Handles a single live asset
class WatchSingle : public Watch {
  public:
	WatchSingle( const ci::fs::path &filePath );

	signals::Connection	connect( const function<void ( const ci::fs::path& )> &callback )	{ return mSignalChanged.connect( callback ); }

	void checkCurrent() override;
	void unwatch( const fs::path &filePath ) override;
	void emitCallback() override;

  private:
	ci::signals::Signal<void ( const ci::fs::path& )>	mSignalChanged;
	ci::fs::path										mFilePath;
	ci::fs::file_time_type								mTimeLastWrite;
};

//! Handles multiple live assets. Takes a vector of fs::paths as argument, result function gets an array of resolved filepaths.
class WatchMany : public Watch {
  public:
	WatchMany( const std::vector<ci::fs::path> &filePaths );

	signals::Connection	connect( const function<void ( const vector<fs::path>& )> &callback )	{ return mSignalChanged.connect( callback ); }

	void checkCurrent() override;
	void unwatch( const fs::path &filePath ) override;
	void emitCallback() override;

	size_t	getNumFiles() const	{ return mFilePaths.size(); }

  private:
	ci::signals::Signal<void ( const std::vector<ci::fs::path>& )>	mSignalChanged;

	std::vector<ci::fs::path>			mFilePaths;
	std::vector<ci::fs::file_time_type>	mTimeStamps;
};

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
// WatchSingle
// ----------------------------------------------------------------------------------------------------

WatchSingle::WatchSingle( const fs::path &filePath )
{
	mFilePath = findFullFilePath( filePath );
	mTimeLastWrite = fs::last_write_time( mFilePath );
}

void WatchSingle::checkCurrent()
{
	if( ! fs::exists( mFilePath ) )
		return;

	// Discard when there are no more connected slots
	if( mSignalChanged.getNumSlots() == 0 ) {
		markDiscarded();
		return;
	}

	auto timeLastWrite = fs::last_write_time( mFilePath );
	if( mTimeLastWrite < timeLastWrite ) {
		mTimeLastWrite = timeLastWrite;
		setNeedsCallback( true );
	}
}

void WatchSingle::unwatch( const fs::path &filePath ) 
{
	if( mFilePath == filePath )
		markDiscarded();
}

void WatchSingle::emitCallback() 
{
	mSignalChanged.emit( mFilePath );
	setNeedsCallback( false );
} 

// ----------------------------------------------------------------------------------------------------
// WatchMany
// ----------------------------------------------------------------------------------------------------

WatchMany::WatchMany( const vector<fs::path> &filePaths )
{
	mFilePaths.reserve( filePaths.size() );
	mTimeStamps.reserve( filePaths.size() );
	for( const auto &fp : filePaths ) {
		auto fullPath = findFullFilePath( fp );
		mFilePaths.push_back( fullPath );
		mTimeStamps.push_back( fs::last_write_time( fullPath ) );
	}
}

void WatchMany::checkCurrent()
{
	// Discard when there are no more connected slots
	if( mSignalChanged.getNumSlots() == 0 ) {
		markDiscarded();
		return;
	}

	const size_t numFiles = mFilePaths.size();
	for( size_t i = 0; i < numFiles; i++ ) {
		const fs::path &fp = mFilePaths[i];
		if( fs::exists( fp ) ) {
			auto timeLastWrite = fs::last_write_time( fp );
			auto &currentTimeLastWrite = mTimeStamps[i];
			if( currentTimeLastWrite < timeLastWrite ) {
				currentTimeLastWrite = timeLastWrite;
				setNeedsCallback( true );
			}
		}
	}
}

void WatchMany::unwatch( const fs::path &filePath ) 
{
	mFilePaths.erase( remove( mFilePaths.begin(), mFilePaths.end(), filePath ), mFilePaths.end() );
	
	if( mFilePaths.empty() )
		markDiscarded();
}

void WatchMany::emitCallback() 
{
	mSignalChanged.emit( mFilePaths );
	setNeedsCallback( false );
} 

// ----------------------------------------------------------------------------------------------------
// FileWatcher
// ----------------------------------------------------------------------------------------------------

namespace {

bool	sWatchingEnabled = true;

} // anonymous namespace

// static
FileWatcher* FileWatcher::instance()
{
	static FileWatcher sInstance;
	return &sInstance;
}

FileWatcher::FileWatcher()
{
	if( sWatchingEnabled )
		startWatching();
}

FileWatcher::~FileWatcher()
{
	stopWatching();
}

// static
void FileWatcher::setWatchingEnabled( bool enable )
{
	if( sWatchingEnabled == enable )
		return;

	sWatchingEnabled = enable;
	if( enable )
		instance()->startWatching();
	else
		instance()->stopWatching();
}

// static
bool FileWatcher::isWatchingEnabled()
{
	return sWatchingEnabled;
}

// static
signals::Connection FileWatcher::load( const fs::path &filePath, const function<void( const fs::path& )> &callback )
{
	auto watch = new WatchSingle( filePath );
	auto conn = watch->connect( callback );

	auto fw = instance();
	lock_guard<recursive_mutex> lock( fw->mMutex );

	fw->mWatchList.emplace_back( watch );
	watch->emitCallback();
	return conn;
}

// static
signals::Connection FileWatcher::load( const vector<fs::path> &filePaths, const function<void ( const vector<fs::path>& )> &callback )
{
	auto watch = new WatchMany( filePaths );
	auto conn = watch->connect( callback );

	auto fw = instance();
	lock_guard<recursive_mutex> lock( fw->mMutex );

	fw->mWatchList.emplace_back( watch );
	watch->emitCallback();
	return conn;
}

// static
signals::Connection FileWatcher::watch( const fs::path &filePath, const function<void( const fs::path& )> &callback )
{
	auto watch = new WatchSingle( filePath );
	auto conn = watch->connect( callback );

	auto fw = instance();
	lock_guard<recursive_mutex> lock( fw->mMutex );

	fw->mWatchList.emplace_back( watch );
	return watch->connect( callback );
}

// static
signals::Connection FileWatcher::watch( const vector<fs::path> &filePaths, const function<void ( const vector<fs::path>& )> &callback )
{
	auto watch = new WatchMany( filePaths );
	auto conn = watch->connect( callback );

	auto fw = instance();
	lock_guard<recursive_mutex> lock( fw->mMutex );

	fw->mWatchList.emplace_back( watch );
	return watch->connect( callback );
}

// static
void FileWatcher::unwatch( const fs::path &filePath )
{
	auto fullPath = findFullFilePath( filePath );

	for( auto &watch : instance()->mWatchList ) {
		watch->unwatch( fullPath );
	}
}

// static
void FileWatcher::unwatch( const vector<fs::path> &filePaths )
{
	for( const auto &filePath : filePaths ) {
		unwatch( filePath );
	}
}

void FileWatcher::startWatching()
{
	if( app::App::get() && ! mUpdateConn.isConnected() )
		mUpdateConn = app::App::get()->getSignalUpdate().connect( bind( &FileWatcher::update, this ) );

	mThreadShouldQuit = false;
	mThread = make_unique<thread>( std::bind( &FileWatcher::threadEntry, this ) );
}

void FileWatcher::stopWatching()
{
	mUpdateConn.disconnect();

	mThreadShouldQuit = true;
	if( mThread && mThread->joinable() ) {
		mThread->join();
		mThread = nullptr;
	}
}

void FileWatcher::threadEntry()
{
	while( ! mThreadShouldQuit ) {
		LOG_UPDATE( "elapsed seconds: " << app::getElapsedSeconds() );
		{
			// try-lock, if we fail to acquire the mutex then we skip this update
			unique_lock<recursive_mutex> lock( mMutex, std::try_to_lock );
			if( ! lock.owns_lock() )
				continue;

			LOG_UPDATE( "\t - updating watches, elapsed seconds: " << app::getElapsedSeconds() );

			try {
				for( auto it = mWatchList.begin(); it != mWatchList.end(); /* */ ) {
					const auto &watch = *it;

					if( watch->isDiscarded() ) {
						it = mWatchList.erase( it );
						continue;
					}

					watch->checkCurrent();
					++it;
				}
			}
			catch( fs::filesystem_error & ) {
				// some file probably got locked by the system. Do nothing this update frame, we'll check again next
			}
		}

		this_thread::sleep_for( chrono::duration<double>( mThreadUpdateInterval ) );
	}
}

void FileWatcher::update()
{
	LOG_UPDATE( "elapsed seconds: " << app::getElapsedSeconds() );

	// try-lock, if we fail to acquire the mutex then we skip this update
	unique_lock<recursive_mutex> lock( mMutex, std::try_to_lock );
	if( ! lock.owns_lock() )
		return;

	LOG_UPDATE( "\t - checking watches, elapsed seconds: " << app::getElapsedSeconds() );

	CI_PROFILE_CPU( "FileWatcher update" );

	for( const auto &watch : mWatchList ) {
		if( watch->needsCallback() )
			watch->emitCallback();
	}

}

const size_t FileWatcher::getNumWatchedFiles() const
{
	size_t result = 0;
	for( const auto &w : mWatchList ) {
		auto many = dynamic_cast<WatchMany *>( w.get() );
		if( many )
			result += many->getNumFiles();
		else
			result += 1;
	}

	return result;
}

} // namespace mason
