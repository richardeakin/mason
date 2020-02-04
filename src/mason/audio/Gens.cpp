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

#include "mason/audio/Gens.h"
#include "cinder/Rand.h"

using namespace ci;
using namespace std;

namespace mason { namespace audio {

// ----------------------------------------------------------------------------------------------------
// GenPinkNoiseNode
// ----------------------------------------------------------------------------------------------------

void GenPinkNoiseNode::initialize()
{
	mB0 = mB1 = mB2 = mB3 = mB4 = mB5 = mB6 = 0;
}

// from: http://www.musicdsp.org/files/pink.txt
void GenPinkNoiseNode::process( ci::audio::Buffer *buffer )
{
	float *data = buffer->getData();
	size_t count = buffer->getSize();

	float b0 = mB0;
	float b1 = mB1;
	float b2 = mB2;
	float b3 = mB3;
	float b4 = mB4;
	float b5 = mB5;
	float b6 = mB6;

	const float gainNormalizer = 0.105f;

	while( count-- ) {
		float white = randFloat( -1, 1 );

		b0 = 0.99886f * b0 + white * 0.0555179f;
		b1 = 0.99332f * b1 + white * 0.0750759f;
		b2 = 0.96900f * b2 + white * 0.1538520f;
		b3 = 0.86650f * b3 + white * 0.3104856f;
		b4 = 0.55000f * b4 + white * 0.5329522f;
		b5 = -0.7616f * b5 - white * 0.0168980f;
		*data++ = ( b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f ) * gainNormalizer;
		b6 = white * 0.115926f;
	}

	mB0 = b0;
	mB1 = b1;
	mB2 = b2;
	mB3 = b3;
	mB4 = b4;
	mB5 = b5;
	mB6 = b6;
}

// ----------------------------------------------------------------------------------------------------
// PeriodicNoiseNode
// ----------------------------------------------------------------------------------------------------

namespace {

// source: http://www.iquilezles.org/apps/soundtoy/soundtoy.js

float grad( int n, float x )
{
	n = (n << 13) ^ n;
	n = (n * (n * n * 15731 + 789221) + 1376312589);
	float res = x;
	if( n & 0x20000000 )
		res = -x;
	return res;
}

float iq_noise( float x )
{
	int i = floor( x );
	float f = x - i;
	float w = f * f * f * ( f * ( f * 6.0 - 15.0 ) + 10.0 );
	float a = grad( i + 0, f + 0.0 );
	float b = grad( i + 1, f - 1.0 );
	return a + ( b - a ) * w;
}
	
}

void PeriodicNoiseNode::process( ci::audio::Buffer *buffer )
{
	float *data = buffer->getData();
	size_t count = buffer->getSize();
	const float noiseFreq = mNoiseFreq;

	for( size_t i = 0; i < count; i++ ) {
		data[i] = iq_noise( mV );

		mV += noiseFreq;
	}
}

} } // namespace mason::audio
