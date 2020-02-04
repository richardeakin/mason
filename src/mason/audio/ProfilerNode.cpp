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

#include "mason/audio/ProfilerNode.h"

#include "cinder/audio/Context.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::audio;

namespace mason { namespace audio {

ProfilerNode::ProfilerNode( const Format &format )
	: Node( format )
{
}

ProfilerNode::~ProfilerNode()
{
}

void ProfilerNode::setBeginProfiler( const ProfilerNodeRef &node )
{
	mBeginProfiler = node;
	mIsBeginProfiler = false;
	node->mIsBeginProfiler = true;
}

void ProfilerNode::enableProcessing()
{
	mFirstRun = true;
}

void ProfilerNode::process( ci::audio::Buffer *buffer )
{
	if( ! mFirstRun ) {
		// store round trip time
		CI_ASSERT( ! mTimerRountrip.isStopped() );
		mTimerRountrip.stop();

		mSecondsRoundtrip = mTimerRountrip.getSeconds();
	}

	mFirstRun = false;
	CI_ASSERT( mTimerRountrip.isStopped() );
	mTimerRountrip.start();

	if( mIsBeginProfiler ) {
		// begin timing between a section in the graph
		CI_ASSERT( mTimerSection.isStopped() );
		mTimerSection.start();
	}
	else if( mBeginProfiler ) {
		// store time between a section in the graph
		auto &timerSection = mBeginProfiler->mTimerSection;
		//CI_ASSERT( ! timerSection.isStopped() );
		timerSection.stop();
		mSecondsInSection = timerSection.getSeconds();
	}
}

} } // namespace mason::audio
