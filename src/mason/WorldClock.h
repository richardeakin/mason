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

 #pragma once

#include "mason/Mason.h"

#include "cinder/Cinder.h"
#include "cinder/Signals.h"

namespace mason {

//! Handles managing the world clock in a manner suitable for things like physics simulations.
//! Uses a fixed timestep by default.
class MA_API WorldClock {
  public:
	WorldClock();

	typedef ci::signals::Signal<void ()> SignalClockStep;

	//! Argument is the current elapsed seconds that the WorldClock has been running.
	SignalClockStep&	getSignalClockStep()	{ return mSignalClockStep; }
	//! Returns the expected delta time in seconds between subsequent calls to the ClockStepSignal.
	double				getTimeStep() const		{ return mTimeStep; }
	//! Returns the elapsed time since the last call to the SignalClockStep
	double				getDeltaTime() const	{ return mDeltaTime; }
	//! Sets the internal time counters to currentTime
	void				setCurrentTime( double currentTime );
	//! Returns the time after the last call to SignalClockStep
	double				getCurrentTime() const;
	//! Sets the target frames per second. In fixed time step mode, this controls the size of the time step.
	void				setTargetFramesPerSecond( double framesPerSecond );
	//! Returns the target frames per second.
	double				getTargetFramesPerSecond() const		{ return mTargetFramesPerSecond; }
	//! Sets the maximum amount of time that will be accounted for in one frame with repeated calls to the SignalClockStep in fixed timestep mode. Default is 100ms.
	void				setMaxAccumulatedTimePerFrame( double maxTime )	{ mMaxAccumulatedTimePerFrame = maxTime; }
	//! Returns the maximum amount of time that will be accounted for in one frame with repeated calls to the SignalClockStep in fixed timestep mode.
	double				getMaxAccumulatedTimePerFrame() const			{ return mMaxAccumulatedTimePerFrame; }
	//! Pauses the world clock. This prevents elapsed time from accumulating but continues to call the SignalClockStep.
	void				setPaused( bool pause = true )			{ mPaused = pause; }
	//! Returns whether the WorldClock is currently paused or not.
	bool				isPaused() const						{ return mPaused; }

	//! If true enables 'fixed timestep mode', so that the ClockStepSignal is called with the same delta time increment
	//! until we've used up the elapsed time since the last update()/
	void setFixedTimeStepEnabled( bool enable )	{ mFixedTimeStep = enable; }
	//! Returns whether 'fixed timestep mode' is enabled (on by default).
	bool isFixedTimeStepEnabled() const			{ return mFixedTimeStep; }
	//! Call this from the application update or draw loop, with the current elapsed time that the application has been running.
	//! \note Keep calling this even when paused, it will be handled internally.
	void update( double currentTime );

  private:
	SignalClockStep	mSignalClockStep;

	bool			mPaused = false;
	bool			mFixedTimeStep = true;
	double			mTargetFramesPerSecond = 0.0;
	double			mTimeStep = 0.0;
	double			mTimeLastUpdated = 0.0;
	double			mAccumulator = 0;
	double			mAccumulatedTime = 0;
	double			mDeltaTime = 0;
	double			mMaxAccumulatedTimePerFrame = 0.1;
};

} // namespace mason
