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
	setChannelMode( ChannelMode::SPECIFIED );
	setNumChannels( 1 );
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
// VcfNode
// ----------------------------------------------------------------------------------------------------
// A port of a [vfc~] pd object, starting with the variant found here: https://github.com/Spaghettis/Spaghettis/blob/master/src/d_vcf.c

typedef float                       t_float;
typedef float                       t_sample;

typedef union {
	t_float     z_f;
	uint32_t    z_i;
} t_rawcast32;

typedef union {
	double      z_d;
	uint32_t    z_i[2];
} t_rawcast64;

#define PD_TWO_PI                   6.283185307179586476925286766559

#define DSP_UNITBIT             1572864.0   // (1.5 * 2^20)

#define DSP_UNITBIT_MSB         0x41380000 
#define DSP_UNITBIT_LSB         0x00000000 

#define PD_RAWCAST64_MSB        1                                                              
#define PD_RAWCAST64_LSB        0

#define PD_MAX(a,b)                 ((a)>(b)?(a):(b))

void *memory_get (size_t n)
{
	void *r = calloc (n < 1 ? 1 : n, 1);

	//PD_ASSERT (r != NULL);
	//PD_ABORT  (r == NULL);

	return r;
}

#define PD_MEMORY_GET(n)                memory_get ((n))

static inline int PD_IS_BIG_OR_SMALL (t_float f)        /* True if exponent falls out (-64, 64) range. */
{
	t_rawcast32 z;
	z.z_f = f;
	return ((z.z_i & 0x20000000) == ((z.z_i >> 1) & 0x20000000)); 
}

#define COSINE_TABLE_SIZE       (1 << 9)    // 512

t_float *cos_tilde_table;

void cos_tilde_initialize (void)
{
	if (!cos_tilde_table) {
		//
		double phase = 0.0;
		double phaseIncrement = PD_TWO_PI / COSINE_TABLE_SIZE;
		int i;

		cos_tilde_table = (t_float *)PD_MEMORY_GET (sizeof (t_float) * (COSINE_TABLE_SIZE + 1));

		for (i = 0; i < COSINE_TABLE_SIZE + 1; i++) {
			cos_tilde_table[i] = (t_float)cos (phase);
			phase += phaseIncrement;
		}
		//
	}
}

static inline t_float dsp_getCosineAtLUT (double position)
{
	t_float f1, f2, f;
	t_rawcast64 z;
	int i;

	z.z_d = position + DSP_UNITBIT;

	i = (int)(z.z_i[PD_RAWCAST64_MSB] & (COSINE_TABLE_SIZE - 1));   /* Integer part. */

	z.z_i[PD_RAWCAST64_MSB] = DSP_UNITBIT_MSB;

	f = (t_float)(z.z_d - DSP_UNITBIT);  /* Fractional part. */

										 /* Linear interpolation. */

	f1 = cos_tilde_table[i + 0];
	f2 = cos_tilde_table[i + 1];

	return (f1 + f * (f2 - f1));
}

#define dsp_getSineAtLUT(index) dsp_getCosineAtLUT ((double)(index) - (COSINE_TABLE_SIZE / 4.0))

VcfNode::VcfNode( const Format &format )
	: Node( format ), mFreq( this, 400 ), mQ( this, 2 )
{
	setChannelMode( ChannelMode::SPECIFIED );
	setNumChannels( 1 );

	cos_tilde_initialize();
}

VcfNode::~VcfNode()
{
}

void VcfNode::initialize()
{
	mConversion = ( 2.0f * M_PI ) / float( getSampleRate() );

	reset();
}

void VcfNode::reset()
{
	mReal = 0;
	mImaginary = 0;
}

void VcfNode::process( ci::audio::Buffer *buffer )
{
	CI_ASSERT_MSG( buffer->getNumChannels() == 1, "multi channel not yet supported" );

	mFreq.eval();
	mQ.eval();

	//float *in1 = (float *)(w[1]);
	//float *in2 = (float *)(w[2]);
	//float *out1 = (float *)(w[3]);
	//float *out2 = (float *)(w[4]);
	//t_vcfctl *c = (t_vcfctl *)(w[5]);
	float *in1 = buffer->getData();
	const float *in2 = mFreq.getValueArray();
	float *out1 = in1;

	//int n = (int)w[6];
	//int i;
	//float re = c->c_re, re2;
	//float im = c->c_im;
	//float q = c->c_q;

	int n = buffer->getNumFrames();
	float re = mReal;
	float im = mImaginary;
	float q = mQ.getValue();
	float k = mConversion;

	Mode mode = mMode;

	double qInverse = (q > 0.0 ? 1.0 / q : 0.0);
	double correction = 2.0 - (2.0 / (q + 2.0));

	while( n-- ) {
		double centerFrequency;
		double r, g;
		double pReal;
		double pImaginary;

		centerFrequency = (*in2++) * k; 
		centerFrequency = PD_MAX (0.0, centerFrequency);

		r = (qInverse > 0.0 ? 1.0 - centerFrequency * qInverse : 0.0);
		r = PD_MAX (0.0, r);
		g = correction * (1.0 - r);

		pReal      = r * dsp_getCosineAtLUT (centerFrequency * (COSINE_TABLE_SIZE / PD_TWO_PI));
		pImaginary = r * dsp_getSineAtLUT   (centerFrequency * (COSINE_TABLE_SIZE / PD_TWO_PI));

		{
			double s = (*in1++);
			double tReal = re;
			double tImaginary = im;

			re = (t_sample)((g * s) + (pReal * tReal - pImaginary * tImaginary));
			im = (t_sample)((pImaginary * tReal + pReal * tImaginary));

			// Output real part in bandpass mode, imaginary in lowpass mode
			// https://lists.puredata.info/pipermail/pd-list/2012-09/097546.html
			*out1++ = ( mode == Mode::BANDPASS ? re : im );
		}
	}

	if (PD_IS_BIG_OR_SMALL (re)) { re = (t_sample)0.0; }
	if (PD_IS_BIG_OR_SMALL (im)) { im = (t_sample)0.0; }

	mReal = re;
	mImaginary = im;
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
