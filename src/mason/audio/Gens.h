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
#include "cinder/audio/GenNode.h"

namespace mason { namespace audio {

class GenPinkNoiseNode : public ci::audio::GenNode {
  protected:
	void initialize() override;
	void process( ci::audio::Buffer *buffer ) override;

  private:
	float mB0, mB1, mB2, mB3, mB4, mB5, mB6;
};

// I'm not really sure what to call this. It wraps the noise function from Inigo Quilez's soundtoy.
class PeriodicNoiseNode : public ci::audio::GenNode {
public:
	PeriodicNoiseNode() : GenNode(), mNoiseFreq( 0.002f ), mV( 0 )	{}

	void setNoiseFreq( float freq )	{ mNoiseFreq = freq; }

protected:
	void process( ci::audio::Buffer *buffer ) override;

private:
	float				mV;
	std::atomic<float>	mNoiseFreq;
};

} } // namespace mason::audio

