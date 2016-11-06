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

#include "mason/WorldClock.h"
#include "cinder/CinderMath.h"
#include "cinder/Log.h"

#define LOG_UPDATE_ENABLED 0

#if LOG_UPDATE_ENABLED
#define LOG_UPDATE( stream )	CI_LOG_I( stream )
#else
#define LOG_UPDATE( stream )	( (void)( 0 ) )
#endif


using namespace std;
using namespace ci;

namespace mason {

WorldClock::WorldClock()
{
	setTargetFramesPerSecond( 60 );
}

void WorldClock::setTargetFramesPerSecond( double framesPerSecond )
{
	mTargetFramesPerSecond = framesPerSecond;
	mTimeStep = 1.0 / mTargetFramesPerSecond;
}

void WorldClock::setCurrentTime( double currentTime )
{
	mTimeLastUpdated = mAccumulatedTime = currentTime;
	mAccumulator = 0;
}

double WorldClock::getCurrentTime() const
{
	return mAccumulatedTime;
}

void WorldClock::update( double currentTime )
{
	// Make sure that mTimeLastUpdated is not ahead of currentTime
	double timeLastUpdated = glm::min<double>( mTimeLastUpdated, currentTime );

	// Calculate elapsed time since last frame.
	double elapsedSeconds = currentTime - timeLastUpdated;
	mTimeLastUpdated = currentTime;

	LOG_UPDATE( "current time: " << currentTime << "s, elapsed: " << elapsedSeconds << "s" << ", time step: " << mTimeStep << "s, paused: " << boolalpha << mPaused << dec );

	// Update the clock, either with a fixed timestep or using 'real time'.
	if( mFixedTimeStep ) {
		// prevent 'spiral of death' by only allowing a max number of clock steps to be called this frame
		// - if we get clamped, mTimeLastUpdated was already set to currentTime so we should be back on track next frame.
		double accum = glm::min<double>( elapsedSeconds, mMaxAccumulatedTimePerFrame );
#if LOG_UPDATE_ENABLED
		if( accum < elapsedSeconds ) {
			LOG_UPDATE( "\t ---- accumulated time clamped to: " << accum << " ----" );
		}
#endif
		mAccumulator += accum;

		mDeltaTime = mPaused ? 0 : mTimeStep;

		while( mAccumulator >= mTimeStep ) {
			mSignalClockStep.emit();

			mAccumulator -= mTimeStep;
			if( ! mPaused )
				mAccumulatedTime += mTimeStep;

			LOG_UPDATE( "\t - [fixed] accumulated time: " << mAccumulatedTime << ", mAccumulator: " << mAccumulator );
		}
	}
	else {
		mDeltaTime = mPaused ? 0 : elapsedSeconds;

		mSignalClockStep.emit();
		if( ! mPaused )
			mAccumulatedTime += elapsedSeconds;

		LOG_UPDATE( "\t - [real-time] accumulated time: " << mAccumulatedTime );
	}
}

} // namespace mason

