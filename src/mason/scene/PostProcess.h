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



// ----------------------------------------------------------------------------------------------------
// Notes:
// - If the 'debug buffer' is enabled and you are drawing with alpha blending enabled,
//   you MUST write 1.0 to the alpha channel in your render frag shader, or the values will all be wrong.
// - Output composited image is always alpha = 1, so this will cover up anything drawn beforehand.
//    - may support transparent output in the future but need to decide on how to handle blending.
// ----------------------------------------------------------------------------------------------------

#pragma once

#define SCENE_MOTION_BLUR_ENABLED 1
#define SCENE_GODRAYS_ENABLED 0
#define SCENE_GLOW_ENABLED 1

#include "mason/scene/PostEffects.h"
#include "mason/Info.h"

// TODO: rename this to RadialBlur or whatever is appropriate
#if SCENE_GODRAYS_ENABLED
#include "mason/scene/GodRays.h"
#endif

#include "cinder/gl/Batch.h"
#include "cinder/gl/Fbo.h"
#include "cinder/Camera.h"

namespace mason::scene {

class DepthOfFieldBokehEffect;
class MotionBlurEffect;
class FXAA;
class SMAA;
//class TAA;

enum class AntiAliasType {
	None,
	MSAA,
	FXAA,
	SMAA,
	SMAA_T2x,
	//TAA,
	NumTypes
};

enum class ColorFormat {
	RGB8,
	RGBA8,
	RGB16,
	RGBA16,
	RGB32,
	RGBA32,
	NumFormats
};

enum class DepthSource {
	DISABLED,
	Z_BUFFER,
	COLOR_ALPHA_CHANNEL
};

//! Manages the post-process effects of a scene
class PostProcess {
public:

	struct Options {
		Options() = default;
		Options( const ma::Info &config );

		Options& gamma( bool b = true )	{ mGamma = b; return *this; }
		Options& bloom( bool b = true )	{ mBloom = b; return *this; }

		Options& colorFormat( ColorFormat fmt )	{ mColorFormat = fmt; return *this; }
		Options& depthSource( DepthSource d )	{ mDepthSource = d; return *this; }

#if SCENE_MOTION_BLUR_ENABLED
		//! Enabling motion blur will automatically enable a floating point velocity buffer
		Options& motionBlur( bool b = true )			{ mMotionBlur = b; return *this; }
#endif

		Options& debugBuffer( bool b = true )			{ mDebugBuffer = b; return *this; }

		Options& bloomDownsampleFactor( float factor )	{ mBloomDownsampleFactor = factor; return *this; }

		Options& config( const ma::Info &config );

		void save( ma::Info &info ) const;
		
	private:
		bool mEnabled = true;
		bool mUIEnabled = false;
		bool mGamma = true;
		bool mBloom = false;
		bool mDepthOfField = false;
		bool mSunRays = true;

		ColorFormat	mColorFormat = ColorFormat::RGB32;
		DepthSource mDepthSource = DepthSource::DISABLED;

		bool mGlowBuffer = false;
		bool mVelocityBuffer = false;
		bool mDebugBuffer = false;

		float mBloomDownsampleFactor = 1;

		float mSunBoost = 10.0f;
		float mSunPower = 2.0f;
		ci::vec2 mSunPos = ci::vec2( 0.915f, 1.0f );
		bool	mSunFromCamera = false;
		
#if SCENE_MOTION_BLUR_ENABLED
		bool	mMotionBlur = false;
#endif

		AntiAliasType			mAntiAliasType = AntiAliasType::None;
		int						mMSAASamples = 8;

		friend class PostProcess;
	};

	static const GLenum COLOR_ATTACHMENT_COLOR			= GL_COLOR_ATTACHMENT0;
	static const GLenum COLOR_ATTACHMENT_GLOW			= GL_COLOR_ATTACHMENT1;
	static const GLenum COLOR_ATTACHMENT_VELOCITY		= GL_COLOR_ATTACHMENT2;
	static const GLenum COLOR_ATTACHMENT_DEBUG			= GL_COLOR_ATTACHMENT3;

	PostProcess( const Options &options = Options() );
	PostProcess( const ma::Info &config );

	~PostProcess();
	
	void load( const ma::Info &info );
	void save( ma::Info &info ) const;

	//! clears all buffers to zero
	void clear();

	//! Will cause FrameBuffers to be reallocated
	void setSize( const ci::ivec2 &size );

	//! disabling will cause preDraw() and postDraw() to be no-op
	void setEnabled( bool b )	{ mOptions.mEnabled = b; }
	//! If disabled, preDraw() and postDraw() are no-op
	bool isEnabled() const		{ return mOptions.mEnabled; }

	//! Set this before drawing
	void setCamera( const ci::CameraPersp &camera )	{ mCam = camera; }
	//!
	const ci::CameraPersp&	getCamera() const	{ return mCam; }

	void			setAntiAliasType( AntiAliasType type );
	AntiAliasType	getAntiAliasType() const	{ return mOptions.mAntiAliasType; }

	void preDraw();
	void postDraw();
	void postDraw( const ci::Rectf &destRect );

	const ci::ivec2&	getSize() const		{ return mSize; }

	//!
	ci::gl::Texture2dRef getColorTexture() { return mFboScene ? mFboScene->getColorTexture() : nullptr; }
	//!
	ci::gl::Texture2dRef getDepthTexture() { return getTexture( GL_DEPTH_ATTACHMENT ); }
	//!
	ci::gl::Texture2dRef getTexture( GLenum attachment );
	//!
	DepthSource getDepthSource() const { return mOptions.mDepthSource; }
	//!
	ColorFormat getColorFormat() const { return mOptions.mColorFormat; }
	//!
	ci::vec4 getDebugPixel( const ci::ivec2 &pixel ) const;
	//!
	void blitTo(const ci::gl::FboRef &fbo ) const;
	//!
	void markAsDirty() const { mFboScene->markAsDirty(); }

#if SCENE_GODRAYS_ENABLED
	// TODO: think I want to remove these, in favor of chaining post effects
	bool isSunRaysEnabled() const	{ return mOptions.mSunRays; }
	void setSunRaysEnabled( bool enable );
#endif

	void updateUI( int devFlags, const ci::Rectf &destRect );
	//! Call this from some other imgui window to add checkboxes for enabling / showing PostProcess window
	void enabledUI();

	ci::signals::Signal<void()> &getBuffersChangedSignal() { return mSignalBuffersChanged; }

private:
	//! configuring buffers is deferred until next preDraw(), but this will flag them as needing configuration
	//! - also deletes the existing framebuffers so they can't be used (eg. in ImGui views) until they are re-created
	void markBuffersNeedConfigure();
	//! Configure the framebuffers according to Options
	void configureBuffers();
	void loadGlsl();

	ci::ivec2				mSize;
	ci::gl::BatchRef		mBatchComposite;
	ci::gl::FboRef			mFboScene, mFboComposite;
	ci::gl::GlslProgRef		mGlslDepthTexturePreview;

	ci::signals::ScopedConnection mConnGlslPostProcess, mConnGlslDepthTexture;
	ci::signals::Signal<void()>   mSignalBuffersChanged;

	ci::CameraPersp			mCam;

	// Defaults are set in Options
	mutable Options mOptions;

#if SCENE_MOTION_BLUR_ENABLED
	std::unique_ptr<MotionBlurEffect>			mMotionBlur; // motion blur pass occurs before bloom / tone-mapping / AA
#endif

	std::unique_ptr<BloomEffect>				mBloom;
	std::unique_ptr<DepthOfFieldBokehEffect>	mDepthOfField;

#if SCENE_GODRAYS_ENABLED
	std::unique_ptr<GodRays>     mGodRays;
	std::unique_ptr<SunRays>     mSunRays;
#endif

	std::unique_ptr<FXAA>	mFXAA;
	std::unique_ptr<SMAA>	mSMAA;
	bool					mBuffersNeedConfigure = false;
};

} // namespace mason::scene
