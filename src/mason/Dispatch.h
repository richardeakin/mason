/*
Copyright (c) 2018, Richard Eakin - All rights reserved.

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

#include "mason/Mason.h"

#include <thread>
#include <functional>
#include <vector>
#include <cstdint>
#include <queue>
#include <mutex>
#include <string>
#include <condition_variable>

namespace mason {

//! Basic DispatchQueue class - a thread-pool with mutex synchronization.
class DispatchQueue {
	typedef std::function<void( void )> FunctionT;

public:
	DispatchQueue( std::string name, size_t thread_cnt = 1 );
	~DispatchQueue();

	//! dispatch and copy, async
	void dispatch( const FunctionT& fn );
	//! dispatch and move, async
	void dispatch( FunctionT&& fn );
	//! dispatches fn on main thread (currently uses app's update loop / io_service)
	void dispatchOnMain( const FunctionT& fn );

	//! Returns number of operations currently in queue.
	size_t getNumQueuedOperations() const;

	// Deleted operations
	DispatchQueue( const DispatchQueue& rhs ) = delete;
	DispatchQueue& operator=( const DispatchQueue& rhs ) = delete;
	DispatchQueue( DispatchQueue&& rhs ) = delete;
	DispatchQueue& operator=( DispatchQueue&& rhs ) = delete;

private:
	void dispatchThreadEntry();

	std::string					mName;
	mutable std::mutex			mMutex;
	std::condition_variable		mCondition;
	std::vector<std::thread>	mThreads;
	std::queue<FunctionT>		mQueue;
	bool						mQuit = false;
};

} // namespace mason
