/*
 Copyright (c) 2015, Richard Eakin - All rights reserved.

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
#include "cinder/audio/Param.h"

namespace mason { namespace audio {

typedef std::shared_ptr<class CompressorNode>		CompressorNodeRef;

class CompressorNode : public ci::audio::Node {
  public:
	CompressorNode( const Format &format = Format() );

	void reset();

	ci::audio::Param* getParamThreshold()		{ return &mParamThreshold; }
	ci::audio::Param* getParamRatio()			{ return &mParamRatio; }
	ci::audio::Param* getParamKnee()			{ return &mParamKnee; }
	ci::audio::Param* getParamAttackTime()		{ return &mParamAttackTime; }
	ci::audio::Param* getParamReleaseTime()		{ return &mParamReleaseTime; }

  protected:
	void initialize() override;
	void process( ci::audio::Buffer *buffer )	override;

  private:

	void setPreDelayTime( float preDelayTime );
	float kneeCurve( float x, float k );
	float saturate( float x, float k );
	float slopeAt( float x, float k );
	float kAtSlope( float desiredSlope );
	float updateStaticCurveParameters( float dbThreshold, float dbKnee, float ratio );

	ci::audio::Param	mParamThreshold, mParamRatio, mParamKnee, mParamAttackTime, mParamReleaseTime;

	float m_detectorAverage;
	float m_compressorGain;

	// Metering
	float m_meteringGain;
	float m_meteringReleaseK;
	
	// Lookahead section.
	unsigned m_lastPreDelayFrames;
	ci::audio::BufferDynamic mPreDelayBuffer;
	int m_preDelayReadIndex;
	int m_preDelayWriteIndex;

	float m_maxAttackCompressionDiffDb;

	// Amount of input change in dB required for 1 dB of output change.
	// This applies to the portion of the curve above m_kneeThresholdDb (see below).
	float m_ratio;
	float m_slope; // Inverse ratio.

	float m_linearThreshold; // The input to output change below the threshold is linear 1:1.
	float m_dbThreshold;

	// m_dbKnee is the number of dB above the threshold before we enter the "ratio" portion of the curve.
	// m_kneeThresholdDb = m_dbThreshold + m_dbKnee
	// The portion between m_dbThreshold and m_kneeThresholdDb is the "soft knee" portion of the curve
	// which transitions smoothly from the linear portion to the ratio portion.
	float m_dbKnee;
	float m_kneeThreshold;
	float m_kneeThresholdDb;
	float m_ykneeThresholdDb;

	// Internal parameter for the knee portion of the curve.
	float m_K;
};

} } // namespace mason::audio
