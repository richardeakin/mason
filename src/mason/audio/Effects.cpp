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

#include "mason/audio/Effects.h"

#include "cinder/audio/Context.h"
#include "cinder/audio/ChannelRouterNode.h"

#include "glm/common.hpp"

using namespace ci;
using namespace ci::audio;

namespace mason { namespace audio {

// ----------------------------------------------------------------------------------------------------
// MoogFilterNode
// ----------------------------------------------------------------------------------------------------

MoogFilterNode::MoogFilterNode( const Format &format )
	: Node( format ), mCutoffFreq( this, 400 ), mQ( this, 2 )
{
}

void MoogFilterNode::initialize()
{
	reset();
}

void MoogFilterNode::reset()
{
	mX1 = mX2 = mX3 = mX4 = 0;
	mY1 = mY2 = mY3 = mY4 = 0;
}

void MoogFilterNode::process( ci::audio::Buffer *buffer )
{
	CI_ASSERT_MSG( buffer->getNumChannels() == 1, "multi channel not yet supported" );

	float *data = buffer->getData();

	size_t n = buffer->getNumFrames();

	mCutoffFreq.eval();
	mQ.eval();

	const float *cutoffFreq = mCutoffFreq.getValueArray();
	const float *quality	= mQ.getValueArray();

	float x1 = mX1;
    float x2 = mX2;
    float x3 = mX3;
    float x4 = mX4;
    float ys1 = mY1;
    float ys2 = mY2;
    float ys3 = mY3;
    float ys4 = mY4;

	while( n-- ) {
		float freq = glm::min( *cutoffFreq++, 8140.0f );
		float q = glm::clamp( *quality++, 0.0f, 4.0f );

		if( freq > 3800 )
			q = q - 0.5f * ( ( freq - 3800 ) / 4300 );

		float pt = freq * 0.01f * 0.0140845f - 0.9999999f;
		float pt1 = ( pt + 1 ) * 0.76923077f;

		float in = *data - q * ys4;

		ys1 = pt1 * ( in  + 0.3f * x1 ) - pt * ys1;
		ys2 = pt1 * ( ys1 + 0.3f * x2 ) - pt * ys2;
		ys3 = pt1 * ( ys2 + 0.3f * x3 ) - pt * ys3;
		ys4 = pt1 * ( ys3 + 0.3f * x4 ) - pt * ys4;

		x1 = in;
		x2 = ys1;
		x3 = ys2;
		x4 = ys3;

		*data++ = ys4;
	}

    mY1 = ys1;
    mY2 = ys2;
    mY3 = ys3;
    mY4 = ys4;
    mX1 = x1;
    mX2 = x2;
    mX3 = x3;
    mX4 = x4;
}

// ----------------------------------------------------------------------------------------------------
// Chorus
// ----------------------------------------------------------------------------------------------------

Chorus::Chorus()
{
}

void Chorus::setupGraph()
{
	CI_ASSERT_MSG( mInput, "no input" );
	CI_ASSERT_MSG( mOutput, "no output" );

	auto ctx = ci::audio::master();

	// setup 3 delays chains, each offset by M_PI / 3 radians
	setupDelayChain( 0, 0 );
	setupDelayChain( 1, 2 * M_PI / 3.0f );
	setupDelayChain( 2, 4 * M_PI / 3.0f );

	auto router = ctx->makeNode( new ci::audio::ChannelRouterNode( ci::audio::Node::Format().channels( 2 ) ) );
	auto leftMix = ctx->makeNode( new ci::audio::Node( ci::audio::Node::Format() ) );
	auto rightMix = ctx->makeNode( new ci::audio::Node( ci::audio::Node::Format() ) );

	// split the input into each of the delay chains
	mInput >> mDelayChains[0].mDelay;
	mInput >> mDelayChains[1].mDelay;
	mInput >> mDelayChains[2].mDelay;

	// route 0 to left, 1 to both, and 2 to right
	mDelayChains[0].mDelay >> leftMix;
	mDelayChains[1].mDelay >> leftMix;
	mDelayChains[1].mDelay >> rightMix;
	mDelayChains[2].mDelay >> rightMix;

	leftMix >> router->route( 0, 0 );
	rightMix >> router->route( 0, 1 );

	router >> mOutput;

	if( mConnectInputToOutput )
		mInput >> mOutput;
}

void Chorus::setupDelayChain( size_t index, float phaseOffset )
{
	auto ctx = ci::audio::master();
	auto &chain = mDelayChains.at( index );

	chain.mDelay = ctx->makeNode( new ci::audio::DelayNode );
	chain.mDelay->setMaxDelaySeconds( 1 ); // TODO: calculate the max delay seconds from other params

	chain.mLfo = ctx->makeNode( new ci::audio::GenSineNode( mLfoRate, ci::audio::Node::Format().autoEnable() ) );
	chain.mLfo->setPhase( phaseOffset );

	chain.mLfoGain = ctx->makeNode( new ci::audio::GainNode( mLfoGain ) );
	chain.mDelaySeconds = ctx->makeNode( new ci::audio::AddNode( mDelaySeconds ) );

	chain.mLfo >> chain.mLfoGain >> chain.mDelaySeconds;
	chain.mDelay->getParamDelaySeconds()->setProcessor( chain.mDelaySeconds );
}

void Chorus::setInput( const ci::audio::NodeRef &node )
{
	mInput = node;
}

void Chorus::setOutput( const ci::audio::NodeRef &node )
{
	mOutput = node;
	setupGraph();
}

void Chorus::setLfoRate( float rate )
{
	mLfoRate = rate;
	for( auto &chain : mDelayChains ) {
		if( chain.mLfo )
			chain.mLfo->setFreq( rate );
	}
}

void Chorus::setLfoGain( float gainLinear )
{
	mLfoGain = gainLinear;
	for( auto &chain : mDelayChains ) {
		if( chain.mLfoGain )
			chain.mLfoGain->setValue( gainLinear );
	}
}

void Chorus::setDelay( float delaySeconds )
{
	mDelaySeconds = delaySeconds;
	for( auto &chain : mDelayChains ) {
		if( chain.mDelaySeconds )
			chain.mDelaySeconds->setValue( delaySeconds );
	}
}

} } // namespace mason::audio
