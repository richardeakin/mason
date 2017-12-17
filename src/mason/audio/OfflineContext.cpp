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

#include "mason/audio/OfflineContext.h"

using namespace std;
using namespace ci;
using namespace ci::audio;

namespace mason { namespace audio {

OutputOfflineNode::OutputOfflineNode( ci::audio::Node::Format &format )
	: ci::audio::OutputNode( format )
{
}

void OutputOfflineNode::initialize()
{
	setupProcessWithSumming();
}

OfflineContext::OfflineContext()
{
}

void OfflineContext::disable()
{
	// Don't really need to do this, but am anyway
	mOutputOffline->disable();
}

const OutputNodeRef& OfflineContext::getOutput()
{
	if( ! mOutputOffline ) {
		mOutputOffline = makeNode<OutputOfflineNode>();
		setOutput( mOutputOffline );
	}
	return Context::getOutput();
}

void OfflineContext::setSampleRate( size_t sampleRate )
{
	// another temp hack in making the output
	getOutput();
	mOutputOffline->mSampleRate = sampleRate;
}

void OfflineContext::setFramesPerBlock( size_t numFrames )
{
	// another temp hack in making the output
	getOutput();
	mOutputOffline->mFramesPerBlock = numFrames;
}

void OfflineContext::setOuputNumChannels( size_t numChannels )
{
	if( numChannels != getOutput()->getNumChannels() ) {
		// recreate output node with correct number of channels
		mOutputOffline = makeNode<OutputOfflineNode>( ci::audio::Node::Format().channels( numChannels ) );
		setOutput( mOutputOffline );
	}
}

void OfflineContext::pull( ci::audio::Buffer *buffer )
{
	CI_ASSERT( buffer->getNumChannels() == getOutput()->getNumChannels() );

	lock_guard<mutex> lock( getMutex() );


	size_t numFramesAvailable = buffer->getNumFrames();
	size_t numFramesWritten = 0;
	auto output = mOutputOffline;

	while( numFramesWritten < numFramesAvailable ) {
		preProcess();

		auto internalBuffer = output->getInternalBuffer();
		internalBuffer->zero();
		output->pullInputs( internalBuffer );

		if( output->checkNotClipping() )
			internalBuffer->zero();

		buffer->copyOffset( *internalBuffer, internalBuffer->getNumFrames(), numFramesWritten, 0 );
		numFramesWritten += internalBuffer->getNumFrames();

		postProcess();
	}
}

} } // namespace mason::audio
