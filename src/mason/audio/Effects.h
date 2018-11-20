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

using MoogFilterNodeRef = std::shared_ptr<class MoogFilterNode>;
using VcfNodeRef = std::shared_ptr<class VcfNode>;

//! \brief Resonant lowpass filter with a 'Moog VCF' sound.
//!
//! Implementation based on the source from Gunter Geiger's [moog~] pd external.
//! A similar algorithm can be found at: http://www.musicdsp.org/showArchiveComment.php?ArchiveID=26
//!
class MoogFilterNode : public ci::audio::Node {
public:
	MoogFilterNode( const Format &format = Format() );

	ci::audio::Param*	getParamFreq()	{ return &mCutoffFreq; }
	ci::audio::Param*	getParamQ()		{ return &mQ; }

	void	setFreq( float freq )		{ mCutoffFreq.setValue( freq ); }
	float	getFreq() const				{ return mCutoffFreq.getValue(); }

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

//! Port of Pd's [vcf~] Node
//!
//! Complex one-pole resonant filter (with audio-rate center frequency input).
//! Default Mode is bandpass, can also be used as a lowpass resonant filter.
class VcfNode : public ci::audio::Node {
public:
	//! These map to the left and right outputs of [vcf~]
	enum class Mode { BANDPASS, LOWPASS };

	VcfNode( const Format &format = Format() );
	~VcfNode();

	ci::audio::Param*	getParamFreq()	{ return &mFreq; }
	ci::audio::Param*	getParamQ()		{ return &mQ; }

	void	setFreq( float freq )		{ mFreq.setValue( freq ); }
	float	getFreq() const				{ return mFreq.getValue(); }
	float	getMinFreq()				{ return 0; }
	float	getMaxFreq()				{ return 20000; }

	void	setQ( float q )				{ mQ.setValue( q ); }
	float	getQ() const				{ return mQ.getValue(); }
	float	getMinQ() const				{ return 0; }
	float	getMaxQ() const				{ return 40; }

	//! Sets the mode, which updates the coefficients so that the frequency response is that of a common type of filter. \see Mode.
	void	setMode( Mode mode )	{ mMode = mode; }
	//! Returns the current mode.
	Mode	getMode() const			{ return mMode; }

	void reset();

protected:
	void initialize() override;
	void process( ci::audio::Buffer *buffer )	override;

private:
	ci::audio::Param	mFreq; // TODO: document this is either cutoff or center, depending on mode
	ci::audio::Param	mQ;
	
	std::atomic<Mode>	mMode = { Mode::BANDPASS };
	float	mReal, mImaginary, mConversion;
};

typedef std::shared_ptr<class Chorus>	ChorusRef;

//! Manages a graph that constructs a stereo chorus effect.
// TODO: this needs enable / disable functionality
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
