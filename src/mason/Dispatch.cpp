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

#include "mason/Dispatch.h"
#include "cinder/Log.h"
#include "cinder/app/AppBase.h"

using namespace std;

namespace mason {

DispatchQueue::DispatchQueue( string name, size_t thread_cnt ) :
	mName( name ), mThreads( thread_cnt )
{
	//CI_LOG_I( "name: " << mName << ", threads: " << thread_cnt );

	for( size_t i = 0; i < mThreads.size(); i++ ) {
		mThreads[i] = thread( &DispatchQueue::dispatchThreadEntry, this );
	}
}

DispatchQueue::~DispatchQueue()
{
	// Signal to dispatch threads that it's time to wrap up
	unique_lock<mutex> lock( mMutex );
	mQuit = true;
	lock.unlock();
	mCondition.notify_all();

	// Wait for threads to finish before we exit
	for( size_t i = 0; i < mThreads.size(); i++ ) {
		if( mThreads[i].joinable() ) {
			printf( "Destructor: Joining thread %zu until completion\n", i );
			mThreads[i].join();
		}
	}
}

void DispatchQueue::dispatch( const FunctionT& fn )
{
	unique_lock<mutex> lock( mMutex );
	mQueue.push( fn );

	// Manual unlocking is done before notifying, to avoid waking up
	// the waiting thread only to block again (see notify_one for details)
	lock.unlock();
	mCondition.notify_all();
}

void DispatchQueue::dispatch( FunctionT&& fn )
{
	unique_lock<mutex> lock( mMutex );
	mQueue.push( move( fn ) );

	// Manual unlocking is done before notifying, to avoid waking up
	// the waiting thread only to block again (see notify_one for details)
	lock.unlock();
	mCondition.notify_all();
}

void DispatchQueue::dispatchThreadEntry()
{
	unique_lock<mutex> lock( mMutex );

	do {
		//Wait until we have data or a quit signal
		mCondition.wait( lock, [this] {
			return ( mQueue.size() || mQuit );
		} );

		//after wait, we own the lock
		if( ! mQuit && mQueue.size() ) {
			auto op = move( mQueue.front() );
			mQueue.pop();

			//unlock now that we're done messing with the queue
			lock.unlock();

			op();

			lock.lock();
		}
	} while( ! mQuit );
}

void DispatchQueue::dispatchOnMain( const FunctionT& fn )
{
	ci::app::AppBase::get()->dispatchAsync( fn );
}

} // namespace mason
