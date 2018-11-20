/*
 Copyright (c) 2017, Richard Eakin - All rights reserved.

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
#include "cinder/audio/Node.h"

#include "cinder/Timer.h"

#include <atomic>

namespace mason { namespace audio {

typedef std::shared_ptr<class ProfilerNode>	ProfilerNodeRef;

class ProfilerNode : public ci::audio::Node {
public:
	ProfilerNode( const Format &format = Format() );
	~ProfilerNode();

	void setBeginProfiler( const ProfilerNodeRef &node );
	const ProfilerNodeRef &getBeginProfiler() const	{ return mBeginProfiler; }

	double		getSecondsRoundtrip() const		{ return mSecondsRoundtrip; }
	double		getSecondsInSection() const		{ return mSecondsInSection; }

protected:
	void enableProcessing() override;
	void process( ci::audio::Buffer *buffer )	override;

private:
	bool		mFirstRun = true;
	ci::Timer	mTimerRountrip, mTimerSection;
	//std::shared_ptr<ci::Timer>	mTimerSection;
	ProfilerNodeRef	mBeginProfiler;

	std::atomic<bool>		mIsBeginProfiler  = { false };
	std::atomic<double>		mSecondsRoundtrip = { -1 };
	std::atomic<double>		mSecondsInSection = { -1 };
};

} } // namespace mason::audio
