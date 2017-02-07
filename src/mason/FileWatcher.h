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

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Noncopyable.h"
#include "cinder/Exception.h"
#include "cinder/Filesystem.h"
#include "cinder/Signals.h"

#include "mason/Export.h"

#include <list>
#include <vector>
#include <mutex>

namespace mason {

typedef std::shared_ptr<class Watch>		WatchRef;
typedef std::shared_ptr<class ScopedWatch>	ScopedWatchRef;

//! Global object for managing live-asset watching.
class MA_API FileWatcher {
  public:
	//! Returns the global instance of FileWatcher
	static FileWatcher* instance();

	static void	setWatchingEnabled( bool enable );
	static bool	isWatchingEnabled();

	//! Loads a single file at \a filePath and adds it to the watch list. Immediately calls \a callback with the resolved path, and also calls it whenever the file has been updated.
	static WatchRef load( const ci::fs::path &filePath, const std::function<void ( const ci::fs::path& )> &callback );
	//! Loads the files in \a filePaths and adds them to the watch list. Immediately calls \a callback with the resolved paths, and also calls it whenever one of the files has been updated.
	static WatchRef load( const std::vector<ci::fs::path> &filePaths, const std::function<void ( const std::vector<ci::fs::path> & )> &callback );
	//! Adds a single file at \a filePath to the watch list. Does not immediately call \a callback, but calls it whenever the file has been updated.
	static WatchRef watch( const ci::fs::path &filePath, const std::function<void ( const ci::fs::path& )> &callback );
	//! Adds the files in \a filePaths to the watch list. Does not immediately call \a callback, but calls it whenever one of the files has been updated.
	static WatchRef watch( const std::vector<ci::fs::path> &filePaths, const std::function<void ( const std::vector<ci::fs::path> & )> &callback );
	//! Adds \a asset to the watch list. Use this if you need to use your own custom Watch type.
	void watch( const WatchRef &watch );

	//! Returns the number of Watch instances being watched.
	const size_t	getNumWatches() const	{ return mWatchList.size(); }
	//! Returns the total number of watched files, taking into account the number of files being watched by a WatchMany
	const size_t	getNumWatchedFiles() const;

  private:
	FileWatcher();

	void	connectUpdate();
	void	update();
	void	removeDiscarded();

	std::list<WatchRef>			mWatchList;
	std::recursive_mutex		mMutex;
	ci::signals::Connection		mUpdateConn;
};

//! Base class for Watch types, which are returned from FileWatcher::load() and watch()
class MA_API Watch : public std::enable_shared_from_this<Watch>, private ci::Noncopyable {
  public:

	virtual ~Watch() = default;

	//! Reloads the asset and calls the callback.
	virtual void reload() = 0;
	//! returns \a true if the asset file is up-to-date, false otherwise.
	virtual bool checkCurrent() = 0;

	void unwatch();

	bool isDiscarded() const		{ return mDiscarded; }
	bool isEnabled() const			{ return mEnabled; }
	void setEnabled( bool b )		{ b ? enable() : disable(); }
	void enable()					{ mEnabled = true; } // TODO: for this to work it would have to update the current time stamp here
	void disable()					{ mEnabled = false; }

  private:
	bool mDiscarded = false;
	bool mEnabled = true;
};

//! Calls Watch::unwatch() when goes out of scope.
// TODO: this should inherit from Watch
// - I don't think that is happening because I return WatchRefs. Is that necessary? maybe instead a Watch can have a handle to hold info
class MA_API ScopedWatch : private ci::Noncopyable {
  public:
	ScopedWatch() = default;
#if defined( CINDER_MSW ) && ( _MSC_VER <= 1800 )
	// can't use =default for move constructor / assignment operator until VS2015
	ScopedWatch( ScopedWatch &&other )
		: mWatch( std::move( other.mWatch ) )
	{}
	ScopedWatch& operator=( ScopedWatch &&rhs )
	{
		mWatch = std::move( rhs.mWatch );
	}
#else
	ScopedWatch( ScopedWatch &&other ) = default;
	ScopedWatch& operator=( ScopedWatch &&rhs ) = default;
#endif
	~ScopedWatch()
	{
		if( mWatch )
			mWatch->unwatch();
	}

	ScopedWatch( const WatchRef &watch )
		: mWatch( watch )
	{}

	ScopedWatch( WatchRef &&liveAsset )
		: mWatch( std::move( liveAsset ) )
	{}

	//! Enables you to assign a WatchRef to this ScopedWatch, unwatching the previous Watch
	ScopedWatch& operator=( const WatchRef &watch )
	{
		if( mWatch )
			mWatch->unwatch();

		mWatch = watch;
		return *this;
	}

	WatchRef operator->() const
	{
		return mWatch;
	}

  private:
	WatchRef mWatch;
};

//! Exception type thrown from errors within FileWatcher
class MA_API FileWatcherException : public ci::Exception {
  public:
	FileWatcherException( const std::string &description )
		: Exception( description )
	{}
};

} // namespace mason
