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

#include "cinder/Cinder.h"
#include "cinder/audio/Node.h"
#include "cinder/audio/GenNode.h"
#include "cinder/audio/Param.h"
#include "cinder/audio/NodeEffects.h"

#include <array>

namespace mason { namespace audio {

typedef std::shared_ptr<class MoogFilterNode>		MoogFilterNodeRef;

//! \brief Resonant lowpass filter with a 'Moog VCF' sound.
//!
//! Implementation based on the source from Gunter Geiger's [moog~] pd external.
//! A similar algorithm can be found at: http://www.musicdsp.org/showArchiveComment.php?ArchiveID=26
//!
class MoogFilterNode : public ci::audio::Node {
public:
	MoogFilterNode( const Format &format = Format() );

	ci::audio::Param*	getParamCutoffFreq()	{ return &mCutoffFreq; }
	ci::audio::Param*	getParamQ()				{ return &mQ; }

	void	setCutoffFreq( float freq )	{ mCutoffFreq.setValue( freq ); }
	float	getCutoffFreq() const		{ return mCutoffFreq.getValue(); }
	float	getMinCutoffFreq()			{ return 0; }
	float	getMaxCutoffFreq()			{ return 8140; }

	void	setQ( float q )				{ mQ.setValue( q ); }
	float	getQ() const				{ return mQ.getValue(); }
	float	getMinQ() const				{ return 0; }
	float	getMaxQ() const				{ return 4; }

	void reset();

protected:
	void initialize() override;
	void process( ci::audio::Buffer *buffer )	override;

private:
	ci::audio::Param	mCutoffFreq;
	ci::audio::Param	mQ;

	float mX1, mX2, mX3, mX4;
	float mY1, mY2, mY3, mY4;
};

typedef std::shared_ptr<class Chorus>	ChorusRef;

//! Manages a graph that constructs a stereo chorus effect.
class Chorus {
  public:
	Chorus();

	void setInput( const ci::audio::NodeRef &node );
	void setOutput( const ci::audio::NodeRef &node );

	void setLfoRate( float rate );
	void setLfoGain( float gainLinear );
	void setDelay( float delaySeconds );

  private:
	struct DelayChain {
		ci::audio::DelayNodeRef		mDelay;
		ci::audio::AddNodeRef		mDelaySeconds;
		ci::audio::GenSineNodeRef	mLfo;
		ci::audio::GainNodeRef		mLfoGain;
	};

	void setupGraph();
	void setupDelayChain( size_t index, float phaseOffset );

	ci::audio::NodeRef			mInput, mOutput;

	std::array<DelayChain, 3>	mDelayChains;

	float mDelaySeconds = 0.3f;
	float mLfoRate = 0.9f;
	float mLfoGain = 0.0008f;
	bool  mConnectInputToOutput = true;
};

inline const ChorusRef& operator>>( const ci::audio::NodeRef &input, const ChorusRef &chorus )
{
	chorus->setInput( input );
	return chorus;
}

inline const ChorusRef& operator>>( const ChorusRef &chorus, const ci::audio::NodeRef &output )
{
	chorus->setOutput( output );
	return chorus;
}

} } // namespace mason::audio
