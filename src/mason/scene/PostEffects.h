/*
Copyright (c) 2017, Richard Eakin project - All rights reserved.

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

#include "cinder/gl/Fbo.h"
#include "cinder/gl/Batch.h"
#include "cinder/Signals.h"
#include "cinder/Exception.h"

namespace mason::scene {

class PostProcess;

class PostEffect {
  public:
	PostEffect( PostProcess *postProcess );
	virtual ~PostEffect();

	//! Processes \t source's color buffer, placing the result in \t dest.
	virtual void process( const ci::gl::FboRef &source ) = 0;

  protected:
	  PostProcess* mPostProcess = nullptr;
};

class BloomEffect : public PostEffect {
  public:
	// TODO: get rid of size param, match what motion blur does
	BloomEffect( PostProcess *postProcess, const ci::ivec2 &size );

	void process( const ci::gl::FboRef &source ) override;

	ci::gl::Texture2dRef	getTexture() const	{ return mFboBlur2->getColorTexture(); }

  private:
	ci::gl::FboRef					mFboBlur1, mFboBlur2;
	ci::gl::GlslProgRef				mGlslBlur;
	ci::signals::ScopedConnection	mConnGlsl;
};

// TODO: either inherit from PostEffect or move to a separate file (anti-alias routines)
class FXAA {
public:
	FXAA();

	//! Processes \t source and draws it at \a bounds directly to the current renderbuffer.
	void draw( const ci::gl::TextureRef &source, const ci::Rectf &destRect );
	//! Processes \t source's color buffer, placing the result in \t dest.
	void apply( const ci::gl::FboRef &source, const ci::gl::FboRef &dest );

	void updateUI();

private:
	ci::gl::BatchRef				mBatch;
	ci::signals::ScopedConnection	mConnGlsl;
};

class SMAA {
  public:
	SMAA();

	//! Processes \t source and draws it at \a bounds directly to the current renderbuffer.
	void draw( const ci::gl::Texture2dRef &source, const ci::Area &bounds );
	//! Processes \t source's color buffer, placing the result in \t dest.
	void apply( const ci::gl::FboRef &source, const ci::gl::FboRef &dest );

	void updateUI();

	ci::gl::Texture2dRef  getEdgePass() const;
	ci::gl::Texture2dRef  getBlendPass() const;

	enum class Preset {
		Low,
		Medium,
		High,
		Ultra 
	};

	static const char* presetToString( Preset preset );

	void setPreset( Preset preset );
	Preset getPreset() const				{ return mPreset; }
	const char* getPresetAsString() const	{ return presetToString( mPreset ); }

  private:
	void	loadGlsl();
	void	createBuffers( int width, int height );

	void	doEdgePass( const ci::gl::Texture2dRef &source );
	void	doBlendPass();

	  
	ci::gl::Fbo::Format   mFboFormat;
	ci::gl::FboRef        mFboEdgePass;
	ci::gl::FboRef        mFboBlendPass;

	ci::gl::BatchRef	mBatchFirstPass;	// edge detection
	ci::gl::BatchRef	mBatchSecondPass;	// blending weight calculation
	ci::gl::BatchRef	mBatchThirdPass;	// neighborhood blending

	// These textures contain look-up tables that speed up the SMAA process.
	ci::gl::Texture2dRef  mAreaTex;
	ci::gl::Texture2dRef  mSearchTex;

	// Size of our buffers, to be passed to the shaders.
	ci::vec4			mMetrics;

	// quality preset setting (see SMAA.glsl)
	Preset				mPreset = Preset::Ultra;

	ci::signals::ConnectionList	mConnections;
};

class PostEffectExc : public ci::Exception {
public:
	PostEffectExc( const std::string &description )
		: Exception( description )
	{}
};

} // namespace mason::scene
