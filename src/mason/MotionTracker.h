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
#include "cinder/Vector.h"

#include <list>

namespace mason {

template <typename T>
class MotionTracker {
public:
	void clear()	{ mStoredPositions.clear(); }
	void storePos( const T &pos, double currentTime );
	T calcVelocity();
	T calcDistance();

	T getFirstPos() const	{ return mFirstTouch.position; }
	//! Returns the positions of the last recorded touch, or T( 0 ) if none have yet been recorded.
	T getLastPos() const;
	//! Returns the time of the last recorded touch, or -1 if none have yet been recorded.
	double getLastTime() const;
	//! Returns the number of stored touches.
	size_t getNumStoredPositions() const	{ return mStoredPositions.size(); }

private:
	struct StoredPos {
		T		pos;
		double	time;
	};

	std::list<StoredPos>	mStoredPositions;
	StoredPos				mFirstTouch;
	size_t					mMaxStoredTouches = 10;
};

template <typename T>
void MotionTracker<T>::storePos( const T &pos, double currentTime )
{
	if( mStoredPositions.size() >= mMaxStoredTouches )
		mStoredPositions.pop_front();

	StoredPos touch;
	touch.pos = pos;
	touch.time = currentTime;
	mStoredPositions.push_back( touch );

	if( mStoredPositions.size() == 1 )
		mFirstTouch = mStoredPositions.front();
}

template <typename T>
T MotionTracker<T>::calcVelocity()
{
	if( mStoredPositions.size() < 2 )
		return T( 0 );

	T touchVelocity = T( 0 );
	int samples = 0;
	auto lastIt = --mStoredPositions.end();
	for( auto it = mStoredPositions.begin(); it != lastIt; ++it ) {
		auto nextIt = it;
		++nextIt;
		double dt = nextIt->time - it->time;
		if( dt > 0.001 ) {
			touchVelocity += ( nextIt->pos - it->pos ) / float( dt );
			samples += 1;
		}
	}

	if( samples > 0 ) {
		touchVelocity /= float( samples );
	}

	return touchVelocity;
}

template <typename T>
T MotionTracker<T>::calcDistance()
{
	return getLastPos() - getFirstPos();
}

template <typename T>
T MotionTracker<T>::getLastPos() const
{
	if( mStoredPositions.empty() )
		return T( 0 );

	return mStoredPositions.back().position;
}

template <typename T>
double MotionTracker<T>::getLastTime() const
{
	if( mStoredPositions.empty() )
		return -1;

	return mStoredPositions.back().eventSeconds;
}

} // namespace mason
