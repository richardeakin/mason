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
#include "cinder/Exception.h"
#include "cinder/Filesystem.h"
#include "cinder/Signals.h"

#include "mason/Export.h"

#include <list>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

namespace mason {

class Watch;

//! Global object for managing live-asset watching.
class MA_API FileWatcher {
  public:
	~FileWatcher();
	
	//! Returns the global instance of FileWatcher
	static FileWatcher* instance();

	//! Globally enables or disables file watching. Must be called on the main thread.
	static void	setWatchingEnabled( bool enable );
	//! Returns whether file watching is enabled or disabled.
	static bool	isWatchingEnabled();

	//! Loads a single file at \a filePath and adds it to the watch list. Immediately calls \a callback with the resolved path, and also calls it whenever the file has been updated.
	static ci::signals::Connection load( const ci::fs::path &filePath, const std::function<void ( const ci::fs::path& )> &callback );
	//! Loads the files in \a filePaths and adds them to the watch list. Immediately calls \a callback with the resolved paths, and also calls it whenever one of the files has been updated.
	static ci::signals::Connection load( const std::vector<ci::fs::path> &filePaths, const std::function<void ( const std::vector<ci::fs::path> & )> &callback );
	//! Adds a single file at \a filePath to the watch list. Does not immediately call \a callback, but calls it whenever the file has been updated.
	static ci::signals::Connection watch( const ci::fs::path &filePath, const std::function<void ( const ci::fs::path& )> &callback );
	//! Adds the files in \a filePaths to the watch list. Does not immediately call \a callback, but calls it whenever one of the files has been updated.
	static ci::signals::Connection watch( const std::vector<ci::fs::path> &filePaths, const std::function<void ( const std::vector<ci::fs::path> & )> &callback );
	//! Removes any watches for \a filePath
	static void unwatch( const ci::fs::path &filePath );
	//! Removes any watches for \a filePaths
	static void unwatch( const std::vector<ci::fs::path> &filePaths );

	//! Returns the number of Watch instances being watched.
	const size_t	getNumWatches() const	{ return mWatchList.size(); }
	//! Returns the total number of watched files, taking into account the number of files being watched by a WatchMany
	const size_t	getNumWatchedFiles() const;

  private:
	FileWatcher();

	void	startWatching();
	void	stopWatching();
	void	threadEntry();
	void	update();

	std::list<std::unique_ptr<Watch>>	mWatchList;
	std::recursive_mutex				mMutex;
	std::unique_ptr<std::thread>		mThread;
	std::atomic<bool>					mThreadShouldQuit;
	std::atomic<double>					mThreadUpdateInterval = { 0.02 };
	ci::signals::Connection				mUpdateConn;
};

//! Exception type thrown from errors within FileWatcher
class MA_API FileWatcherException : public ci::Exception {
  public:
	FileWatcherException( const std::string &description )
		: Exception( description )
	{}
};

} // namespace mason
