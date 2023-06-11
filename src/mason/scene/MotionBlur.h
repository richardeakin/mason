/*
Copyright (c) 2017-23, Richard Eakin project - All rights reserved.

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

#include "mason/scene/PostEffects.h"
#include "cinder/gl/Batch.h"
#include "cinder/Signals.h"
#include "cinder/Exception.h"

namespace mason::scene {

//! Screen-space post-processed motion blur, ported from G3D
class MotionBlurEffect : public PostEffect {
public:
	MotionBlurEffect( PostProcess *postProcess );

	void process( const ci::gl::FboRef &source ) override;

	void updateUI();

	ci::gl::Texture2dRef	getTextureTileMinMaxHorizontal() const	{ return mFboTileMinMaxTemp->getColorTexture(); }
	ci::gl::Texture2dRef	getTextureTileMinMax() const	{ return mFboTileMinMax->getColorTexture(); }
	ci::gl::Texture2dRef	getTextureNeighorMinMax() const	{ return mFboNeighborMinMax->getColorTexture(); }
	ci::gl::Texture2dRef	getTextureReconstruct() const	{ return mFboReconstruct->getColorTexture(); }

	ci::gl::FboRef	getDestFbo() const	{ return mFboReconstruct; }

	int getNumSamples() const	{ return mNumSamples; }
	void setNumSamples( int samples );
	void setVelocityScale( float s )	{ mVelocityScale = s; }

private:
	void updateBuffers( const ci::ivec2 &size, int maxBlurRadiusPixels );

	ci::gl::FboRef					mFboTileMinMaxTemp, mFboTileMinMax, mFboNeighborMinMax, mFboReconstruct;
	ci::gl::GlslProgRef				mGlslTileMinMaxHorizontal, mGlslTileMinMax, mGlslNeighborMinMax, mGlslReconstruct;
	ci::signals::ConnectionList		mConnections;

	// ------------
	// params 

	// number of samples used when calculating motion blur, always rounded up to the nearest odd number.
	int		mNumSamples = 31;
	// objects are blurred over at most this distance as a fraction	of the screen dimension along the dominant field of view
	// - changing this will cause the buffers to be resized
	float	mMaxBlurDiameterFraction = 0.1f;
	// Fraction of the frame interval during which the shutter is open. Larger values create more motion blur.
	float	mExposureTimeFraction    = 0.75f;
	// scale whatever velocity values are in the buffer by this
	float	mVelocityScale = 1.0f;
};

} // namespace mason::scene