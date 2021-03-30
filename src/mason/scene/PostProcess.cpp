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


#pragma optimize( "", off )

#include "mason/scene/PostProcess.h"
#include "mason/scene/DepthOfField.h"
#include "mason/scene/MotionBlur.h"
#include "mason/Assets.h"
#include "mason/Common.h"
#include "mason/Profiling.h"
#include "mason/extra/ImGuiStuff.h"
#include "mason/extra/ImGuiTexture.h"

#include "cinder/Log.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/ConstantConversions.h" // TODO: add to gl.h
#include "cinder/app/AppBase.h"
#include <glm/gtc/epsilon.hpp>

#define LOG_BUFFERS( stream )	CI_LOG_I( stream )
//#define LOG_BUFFERS( stream )	((void)0)

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason::scene {

namespace {

const uint8_t TEXTURE_UNIT_COLOR = 0;
const uint8_t TEXTURE_UNIT_DEPTH = 1;
const uint8_t TEXTURE_UNIT_GLOW = 2;
const uint8_t TEXTURE_UNIT_VELOCITY = 3;


const char *antiAliasTypeToString( AntiAliasType type )
{
	switch( type ) {
		case AntiAliasType::None:	return "none";
		case AntiAliasType::MSAA:	return "msaa";
		case AntiAliasType::FXAA:	return "fxaa";
		case AntiAliasType::SMAA:	return "smaa";
		default: break;

	}

	CI_ASSERT_NOT_REACHABLE();
	return "(unknown)";
}

AntiAliasType antiAliasTypeFromString( const std::string &str )
{
	if( str == "none" )			return AntiAliasType::None;
	else if( str == "msaa" )	return AntiAliasType::MSAA;
	else if( str == "fxaa" )	return AntiAliasType::FXAA;
	else if( str == "smaa" )	return AntiAliasType::SMAA;

	//CI_ASSERT_NOT_REACHABLE();
	return AntiAliasType::NumTypes;
}

ColorFormat colorFormatFromString( const std::string &str )
{
	if( str == "rgb8" )			return ColorFormat::RGB8;
	else if( str == "rgba8" )	return ColorFormat::RGBA8;
	else if( str == "rgb16" )	return ColorFormat::RGB16;
	else if( str == "rgba16" )	return ColorFormat::RGBA16;
	else if( str == "rgb32" )	return ColorFormat::RGB32;
	else if( str == "rgba32" )	return ColorFormat::RGBA32;

	//CI_ASSERT_NOT_REACHABLE();
	return ColorFormat::NumFormats;
}

const char *colorFormatToString( ColorFormat format )
{
	switch( format ) {
		case ColorFormat::RGB8:		return "rgb8";
		case ColorFormat::RGBA8:	return "rgba8";
		case ColorFormat::RGB16:	return "rgb16";
		case ColorFormat::RGBA16:	return "rgba16";
		case ColorFormat::RGB32:	return "rgb32";
		case ColorFormat::RGBA32:	return "rgba32";
		default: break;

	}

	CI_ASSERT_NOT_REACHABLE();
	return "(unknown)";
}

GLint colorFormatToGL( ColorFormat format )
{
	switch( format ) {
		case ColorFormat::RGB8:		return GL_RGB8;
		case ColorFormat::RGBA8:	return GL_RGBA8;
		case ColorFormat::RGB16:	return GL_RGB16F;
		case ColorFormat::RGBA16:	return GL_RGBA16F;
		case ColorFormat::RGB32:	return GL_RGB32F;
		case ColorFormat::RGBA32:	return GL_RGBA32F;
		default: break;

	}

	CI_ASSERT_NOT_REACHABLE();
	return GL_ZERO;
}


} // anonymous namespace

PostProcess::Options::Options( const ma::Info &config )
{
	this->config( config );
}

// TODO: rename config -> info for consistency
PostProcess::Options &PostProcess::Options::config( const ma::Info &config )
{
	mEnabled = config.get( "enabled", mEnabled );
	mUIEnabled = config.get( "ui", mUIEnabled );
	mGamma = config.get( "gamma", mGamma );
	mGlowBuffer = config.get( "glowBuffer", mGlowBuffer );
	mVelocityBuffer = config.get( "velocityBuffer", mVelocityBuffer );
	mDebugBuffer = config.get( "debugBuffer", mDebugBuffer );

	// main color buffer
	ColorFormat defaultColorFormat = mColorFormat;
	const string colorFormatStr = config.get<string>( "colorFormat", colorFormatToString( defaultColorFormat ) );
	mColorFormat = colorFormatFromString( colorFormatStr );
	if( mColorFormat == ColorFormat::NumFormats ) {
		CI_LOG_E( "unexpected color format string: " << colorFormatStr );
		mColorFormat = defaultColorFormat;
	}

	// depth buffer
	const string depthSource = config.get<string>( "depthSource", "disabled" );
	if( depthSource == "z buffer" ) {
		mDepthSource = DepthSource::Z_BUFFER;
	}
	else if( depthSource == "color alpha" ) {
		mDepthSource = DepthSource::COLOR_ALPHA_CHANNEL;
	}

	// TODO: sometimes I need an alpha channel and this is making that strange.
	// - either the distance in the velocity buffer or need to sort out writing to z buffer when raymarching
	if( mDepthSource == DepthSource::COLOR_ALPHA_CHANNEL && mColorFormat == ColorFormat::RGBA32 ) {
		CI_LOG_E( "incompatible: mDepthSource == DepthSource::COLOR_ALPHA_CHANNEL && mColorFormat == ColorFormat::RBGA32" );
	}

	// Anti-Aliasing
	string antiAliasStr = config.get<string>( "antiAlias", antiAliasTypeToString( mAntiAliasType ) );
	mAntiAliasType = antiAliasTypeFromString( antiAliasStr );
	if( mAntiAliasType == AntiAliasType::NumTypes ) {
		CI_LOG_E( "unexpected antiAlias string: " << antiAliasStr );
		mAntiAliasType = AntiAliasType::None;
	}
	mMSAASamples = config.get( "msaaSamples", mMSAASamples );

	// BloomEffect
	mBloom = config.get( "bloom", mBloom );

	// MotionBlurEffect
	mMotionBlur = config.get( "motionBlur", mMotionBlur );

	// Depth Of Field
	mDepthOfField = config.get( "depthOfField", mDepthOfField );

	// Godrays
	mSunRays = config.get( "sunRays", mSunRays );
	mSunBoost = config.get( "sunBoost", mSunBoost );
	mSunPower = config.get( "sunPower", mSunPower );
	mSunPos = config.get( "sunPos", mSunPos );
	mSunFromCamera = config.get( "sunFromCamera", mSunFromCamera );

	mBloomDownsampleFactor = config.get( "bloomDownsampleFactor", mBloomDownsampleFactor );

	return *this;
}

void PostProcess::Options::save( ma::Info &info ) const
{
	info["enabled"] = mEnabled;
	info["ui"] = mUIEnabled;
	info["antiAlias"] = antiAliasTypeToString( mAntiAliasType );
	info["msaaSamples"] = mMSAASamples;
	info["velocityBuffer"] = mVelocityBuffer;
	info["motionBlur"] = mMotionBlur;
	info["depthOfField"] = mDepthOfField;
	info["gamma"] = mGamma;
	info["bloom"] = mBloom;
	info["sunRays"] = mSunRays;
	info["sunBoost"] = mSunBoost;
	info["sunPower"] = mSunPower;
	info["sunPos"] = mSunPos;
	info["sunFromCamera"] = mSunFromCamera;
	info["glowBuffer"] = mGlowBuffer;
	info["debugBuffer"] = mDebugBuffer;
	info["bloomDownsampleFactor"] = mBloomDownsampleFactor;
	info["colorFormat"] = colorFormatToString( mColorFormat );
	info["depthSource"] = mDepthSource == DepthSource::Z_BUFFER ? "z buffer" : mDepthSource == DepthSource::COLOR_ALPHA_CHANNEL ? "color alpha" : "disabled";
}

PostProcess::PostProcess( const Options &options )
	: mOptions( options )
{
	loadGlsl();

	// set to none at startup since AA hasn't yet been initialized, then we will init AA if needed
	auto aa = mOptions.mAntiAliasType;
	mOptions.mAntiAliasType = AntiAliasType::None;
	setAntiAliasType( aa );

	markBuffersNeedConfigure();
}

PostProcess::PostProcess( const ma::Info &config )
	: PostProcess( Options( config ) )
{
}

PostProcess::~PostProcess()
{
}

void PostProcess::load( const ma::Info &info )
{
	mOptions = info;
	loadGlsl();
	markBuffersNeedConfigure();
}

void PostProcess::save( ma::Info &info ) const
{
	mOptions.save( info );
}

void PostProcess::setSize( const ci::ivec2 &size )
{
	if( mSize == size ) {
		return;
	}

	mSize = size;
	markBuffersNeedConfigure();
}

void PostProcess::markBuffersNeedConfigure()
{
	mBuffersNeedConfigure = true;

	if( mFboScene || mFboComposite ) {
		LOG_BUFFERS( "deleting buffers" );
	}
	mFboScene = nullptr;
	mFboComposite = nullptr;
}

vector<GLenum> drawBuffers;

void PostProcess::configureBuffers()
{
	auto fboFormat = gl::Fbo::Format();
	//vector<GLenum> drawBuffers;
	drawBuffers.clear();

	LOG_BUFFERS( "color format: " << colorFormatToString( mOptions.mColorFormat ) );

	// main color attachment
	GLint internalColorFormat = colorFormatToGL( mOptions.mColorFormat );
	{
		// TODO: update colorFormatToGL to handle packing depth into alpha channel in floating point buffers
		// - maybe ColorFormat should handle this entirely (ex. ColorFormat::RGB32_DEPTH32
		//if( mOptions.mDepthSource == DepthSource::COLOR_ALPHA_CHANNEL || mOptions.mColorFormat == ColorFormat::RGBA32 ) {
		//	internalFormat = GL_RGBA32F;
		//}
		auto format = gl::Texture::Format()
			.internalFormat( internalColorFormat )
			.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		;

		format.setLabel( "PostProcess Color" );
		fboFormat.attachment( COLOR_ATTACHMENT_COLOR, gl::Texture2d::create( mSize.x, mSize.y, format ) );
		drawBuffers.push_back( COLOR_ATTACHMENT_COLOR );
	}

	if( mOptions.mDepthSource == DepthSource::Z_BUFFER ) {
		fboFormat.depthTexture();
	}
	else if( mOptions.mDepthSource == DepthSource::DISABLED ) {
		fboFormat.disableDepth();
	}

	if( mOptions.mBloom ) {
		mOptions.mGlowBuffer = true; // force glow attachment, used by bloom effect

		vec2 bloomSize = mSize;
		mOptions.mBloomDownsampleFactor = max( 0.01f, mOptions.mBloomDownsampleFactor );
		bloomSize /= mOptions.mBloomDownsampleFactor;
		mBloom = make_unique<BloomEffect>( this, bloomSize );

		CI_LOG_I( "mSize: " << mSize << ", bloomSize: " << bloomSize );
	}

#if SCENE_GLOW_ENABLED
	if( mOptions.mGlowBuffer ) {
		// add HDR attachment for oGlowMap
		auto format = gl::Texture::Format()
			.internalFormat( internalColorFormat )
			.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		;

		format.setLabel( "PostProcess Glow" );

		fboFormat.attachment( COLOR_ATTACHMENT_GLOW, gl::Texture2d::create( mSize.x, mSize.y, format ) );
		drawBuffers.push_back( COLOR_ATTACHMENT_GLOW );
	}
	else {
		mBloom = nullptr;
		drawBuffers.push_back( GL_NONE );
	}
#endif

#if SCENE_MOTION_BLUR_ENABLED
	if( mOptions.mMotionBlur ) {
		mOptions.mVelocityBuffer = true; // force velocity buffer attachment
		mMotionBlur = make_unique<MotionBlurEffect>( this );
	}

	if( mOptions.mVelocityBuffer ) {
		// add attachment for velocity buffer
		auto format = gl::Texture::Format()
			.internalFormat( GL_RG32F ) // TODO: switch to 16F buffer, no need for 32F
			.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		;

		format.setLabel( "PostProcess Velocity" );

		fboFormat.attachment( COLOR_ATTACHMENT_VELOCITY, gl::Texture2d::create( mSize.x, mSize.y, format ) );
		drawBuffers.push_back( COLOR_ATTACHMENT_VELOCITY );
	}
	else {
		drawBuffers.push_back( GL_NONE );
	}
#endif

	if( mOptions.mDepthOfField ) {
		mDepthOfField = make_unique<DepthOfFieldBokehEffect>( this );
	}

#if SCENE_GODRAYS_ENABLED

	if( mOptions.mSunRays ) {
		mSunRays = make_unique<SunRays>( this );
		mSunRays->setSunBoost( mOptions.mSunBoost );
		mSunRays->setSunPower( mOptions.mSunPower );
		mSunRays->setSunPosition( mOptions.mSunPos );
		mSunRays->enableSunFromCamera( mOptions.mSunFromCamera );
	}
	else {
		mSunRays = nullptr;
	}
#endif

	if( mOptions.mDebugBuffer ) {
		auto format = gl::Texture::Format()
			.internalFormat( GL_RGBA32F )
			.minFilter( GL_NEAREST ).magFilter( GL_NEAREST  )
		;

		format.setLabel( "PostProcess Debug" );

		fboFormat.attachment( COLOR_ATTACHMENT_DEBUG, gl::Texture2d::create( mSize.x, mSize.y, format ) );
		drawBuffers.push_back( COLOR_ATTACHMENT_DEBUG );
	}
	else {
		drawBuffers.push_back( GL_NONE );
	}

	if( mOptions.mAntiAliasType == AntiAliasType::MSAA ) {
		CI_LOG_I( "configuring MSAA with " << mOptions.mMSAASamples << " samples.." );
		fboFormat.samples( mOptions.mMSAASamples );
	}

	try {

		mFboScene = gl::Fbo::create( mSize.x, mSize.y, fboFormat );
		mFboScene->setLabel( "PostProcess Scene Fbo" );
	}
	catch( gl::FboException &exc ) {
		CI_LOG_EXCEPTION( "Failed to create mFboScene.", exc );
		throw exc;
	}

	// note: ci::gl::Fbo::setDrawBuffers() doesn't account for disabled slots (they need to be specified with GL_NONE)
	// - so I'm calling gl::drawBuffers() once more here, although this could be handled by the Fbo class internally.
	// - see Description section here: https://www.khronos.org/opengl/wiki/GLAPI/glDrawBuffers
	{
		gl::ScopedFramebuffer fboScope( mFboScene );
		gl::drawBuffers( drawBuffers.size(), drawBuffers.data() );
	}

	// Create compositing Fbo
	{
		auto fboFormatComposite = gl::Fbo::Format()
			.disableDepth();
		;

		auto texFormat = gl::Texture::Format()
			//.internalFormat( GL_RGB8 ) // TODO: can save memory in some cases with GL_RGB8 if both alpha and FXAA / SMAA are disabled
			.label( "PostProcess Composite" )
			.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		;

		fboFormatComposite.attachment( GL_COLOR_ATTACHMENT0, gl::Texture2d::create( mSize.x, mSize.y, texFormat ) );

		mFboComposite = gl::Fbo::create( mSize.x, mSize.y, fboFormatComposite );
		mFboComposite->setLabel( "PostProcess Composite Fbo" );
	}
	
	mBuffersNeedConfigure = false;

	mSignalBuffersChanged.emit();
}

void PostProcess::clear()
{
	vec4 zeroes( 0, 0, 0, 0 );

	if( mFboScene ) {
		// TODO: cover this for 8 bit and 16 bit buffers
		GLenum texFormat = mOptions.mColorFormat == ColorFormat::RGBA32 ? GL_RGBA : GL_RGB;
		glClearTexImage( mFboScene->getColorTexture()->getId(), 0, texFormat, GL_FLOAT, &zeroes[0] );

#if SCENE_GLOW_ENABLED
		if( mOptions.mGlowBuffer ) {
			auto glowTex = getTexture( COLOR_ATTACHMENT_GLOW );
			glClearTexImage( glowTex->getId(), 0, texFormat, GL_FLOAT, &zeroes[0] );
		}
#endif
	}


	CI_LOG_I( "complete." );
}


void PostProcess::loadGlsl()
{
	auto format = gl::GlslProg::Format();

	if( mOptions.mGamma ) // TODO: should also defnie if AA is FXAA / SMAA (though currently this define isn't being used in composite.frag
		format.define( "GAMMA_ENABLED" );
	if( mOptions.mBloom )
		format.define( "BLOOM_ENABLED" );

	format.define( "AA_TYPE", to_string( (int)mOptions.mAntiAliasType ) );

	mConnGlslPostProcess = ma::assets()->getShader( "mason/passthrough.vert", "mason/post/composite.frag", format, [this]( gl::GlslProgRef glsl ) {
		glsl->uniform( "uTexColor", TEXTURE_UNIT_COLOR );

		if( mOptions.mDepthSource != DepthSource::DISABLED ) {
			glsl->uniform( "uTexDepth", TEXTURE_UNIT_DEPTH );
		}
		if( mOptions.mBloom ) {
			glsl->uniform( "uTexGlow", TEXTURE_UNIT_GLOW );
		}

#if SCENE_MOTION_BLUR_ENABLED
		glsl->uniform( "uTexVelocity", TEXTURE_UNIT_VELOCITY );
#endif

		if( mBatchComposite )
			mBatchComposite->replaceGlslProg( glsl );
		else {
			auto geom = geom::Rect( Rectf( 0, 0, 1, 1 ) );
			mBatchComposite = gl::Batch::create( geom, glsl );
		}
	} );
}

void PostProcess::preDraw()
{
	if( ! mOptions.mEnabled )
		return;

	if( mBuffersNeedConfigure && mSize.x != 0 && mSize.y != 0 )
		configureBuffers();

	if( ! mFboScene || ! mFboComposite )
		return;

	gl::context()->pushFramebuffer( mFboScene );

	// TODO: figure if it's indeed best to call glDrawBuffers() every frame
	// - seems to be fixing the issue with msaa + motion blur and no glow buffer attachment
	gl::drawBuffers( drawBuffers.size(), drawBuffers.data() );

	gl::pushViewport( 0, 0, mFboScene->getWidth(), mFboScene->getHeight() );

	gl::pushMatrices();
	gl::setMatricesWindow( mFboScene->getSize() );
}

void PostProcess::postDraw()
{
	if( ! mOptions.mEnabled || ! mFboScene || ! mFboComposite ) {
		return;
	}

	postDraw( mFboScene->getBounds() );
}

void PostProcess::postDraw( const Rectf &destRect )
{
	if( ! mOptions.mEnabled || ! mFboScene || ! mFboComposite ) {
		return;
	}

	gl::context()->popFramebuffer();
	gl::popViewport();
	gl::popMatrices();

	auto srcFbo = mFboScene;

#if SCENE_MOTION_BLUR_ENABLED
	if( mMotionBlur ) {
		MA_PROFILE( "PostProcess - Motion Blur" );

		mMotionBlur->process( srcFbo );
		srcFbo = mMotionBlur->getDestFbo();
	}
#endif

	if( mDepthOfField ) {
		MA_PROFILE( "PostProcess - Depth of Field" );
		mDepthOfField->process( srcFbo );
		srcFbo = mDepthOfField->getDestFbo();
	}

	if( mBloom ) {
		MA_PROFILE( "PostProcess - Bloom" );

		mBloom->process( srcFbo );
	}

#if SCENE_GODRAYS_ENABLED

	if( mSunRays ) {
		MA_PROFILE( "PostProcess - SunRays" );

		mSunRays->setCamera( mCam );
		mSunRays->process( srcFbo );
		srcFbo = mSunRays->getFbo();
	}
#endif

	// disable blending and depth, for both composite and AA passes
	gl::ScopedBlend blendScope( false );
	gl::ScopedDepth depthScope( false );

	// Composite scene
	if( mBatchComposite ) {
		MA_PROFILE( "PostProcess - Composite" );

		gl::ScopedFramebuffer scopedFbo( mFboComposite );
		gl::ScopedViewport    scopedViewport( 0, 0, mFboComposite->getWidth(), mFboComposite->getHeight() );
		gl::ScopedMatrices    scopedMatrices;
		gl::setMatricesWindow( mFboComposite->getSize() );

		gl::ScopedTextureBind texColorScope( srcFbo->getColorTexture(), TEXTURE_UNIT_COLOR );

		// writeImage( "scene.png", destFbo->getColorTexture()->createSource() );

#if SCENE_MOTION_BLUR_ENABLED
		gl::Texture2dRef velocityTex;
		if( mOptions.mVelocityBuffer ) {
			// TODO: remove from compositing scene once the MotionBlur effect is finished
			// - or perhaps: I could do the reconstruction in this shader too, perhaps as an include
			// - this would match the same usage of BloomEffect and reduce one extra RGB16f fullscreen texture, which is alot in big apps
			// - but should hold off on that until DoF is in here, too
			velocityTex = mFboScene->getTexture2d( COLOR_ATTACHMENT_VELOCITY );
			gl::context()->pushTextureBinding( velocityTex->getTarget(), velocityTex->getId(), TEXTURE_UNIT_VELOCITY );
		}
#endif

		if( mBloom ) {
			gl::context()->pushTextureBinding( mBloom->getTexture()->getTarget(), mBloom->getTexture()->getId(), TEXTURE_UNIT_GLOW );
		}

		// draw full-screen quad
		{
			gl::ScopedModelMatrix modelScope;
			gl::scale( mSize.x, mSize.y, 0 );
			mBatchComposite->draw();
		}

#if SCENE_MOTION_BLUR_ENABLED
		if( mOptions.mVelocityBuffer ) {
			gl::context()->popTextureBinding( velocityTex->getTarget(), TEXTURE_UNIT_VELOCITY );
		}
#endif

		if( mBloom ) {
			gl::context()->popTextureBinding( mBloom->getTexture()->getTarget(), TEXTURE_UNIT_GLOW );
		}

		srcFbo = mFboComposite;
	}

	// Anti-Aliasing
	if( mOptions.mAntiAliasType == AntiAliasType::None || mOptions.mAntiAliasType == AntiAliasType::MSAA ) {
		gl::draw( srcFbo->getColorTexture(), Area( destRect ) );
	}
	else {
		gl::ScopedColor colorScope( Color::white() );

		// Both FXAA and SMAA require source to use GL_LINEAR for min and mag filter
		CI_ASSERT( srcFbo->getFormat().getColorTextureFormat().getMinFilter() == GL_LINEAR );
		CI_ASSERT( srcFbo->getFormat().getColorTextureFormat().getMagFilter() == GL_LINEAR );

		if( mOptions.mAntiAliasType == AntiAliasType::FXAA && mFXAA ) {
			MA_PROFILE( "PostProcess - FXAA" );
			mFXAA->draw( srcFbo->getColorTexture(), Area( destRect ) );
		}
		if( mOptions.mAntiAliasType == AntiAliasType::SMAA && mSMAA ) {
			MA_PROFILE( "PostProcess - SMAA" );
			mSMAA->draw( srcFbo->getColorTexture(), Area( destRect ) );

			//gl::draw( mSMAA->getEdgePass(), destRect );
		}
	}
}


void PostProcess::setAntiAliasType( AntiAliasType type )
{
	if( mOptions.mAntiAliasType == type )
		return;

	switch( type ) {
		case AntiAliasType::None:
		case AntiAliasType::MSAA: {
			mFXAA = nullptr;
			mSMAA = nullptr;
		}
		break;
		case AntiAliasType::FXAA: {
			mFXAA = make_unique<FXAA>();
			mSMAA = nullptr;
		}
		break;
		case AntiAliasType::SMAA: {
			mFXAA = nullptr;
			mSMAA = make_unique<SMAA>();
		}
		break;
		default:
		CI_ASSERT_NOT_REACHABLE();
	}

	mOptions.mAntiAliasType = type;
	markBuffersNeedConfigure();

	if( mBatchComposite ) {
		loadGlsl();
	}
}

gl::Texture2dRef PostProcess::getTexture( GLenum attachment )
{
	return mFboScene->getTexture2d( attachment );
}

vec4 PostProcess::getDebugPixel( const ci::ivec2 &pixel ) const
{
	if( !mOptions.mDebugBuffer || !mFboScene )
		return vec4( -1 );

	auto area = Area( pixel, pixel + ivec2( 1 ) );
	auto surface = mFboScene->readPixels32f( area, COLOR_ATTACHMENT_DEBUG );
	return (vec4)surface.getPixel( ivec2( 0 ) );
}

void PostProcess::blitTo( const gl::FboRef &fbo ) const
{
	mFboScene->blitTo( fbo, mFboScene->getBounds(), fbo->getBounds() );
}

#if SCENE_GODRAYS_ENABLED
void PostProcess::setSunRaysEnabled( bool enable )
{
	mOptions.mSunRays = enable;
	if( mOptions.mSunRays ) {
		markBuffersNeedConfigure();
	}
	mSunRays = nullptr;
}
#endif

// ----------------------------------------------------------------------------------------------------
// ImGui
// ----------------------------------------------------------------------------------------------------

void PostProcess::updateUI( int devFlags, const ci::Rectf &destRect )
{
	if( ! mOptions.mUIEnabled ) {
		return;
	}

	if( ! im::Begin( "Post Process", &mOptions.mUIEnabled ) ) {
		im::End();
		return;
	}

	im::Text( "size: [%d, %d], color format: %s", mSize.x, mSize.y, colorFormatToString( mOptions.mColorFormat ) );
	im::Checkbox( "enabled", &mOptions.mEnabled );
	if( im::Checkbox( "gamma", &mOptions.mGamma ) ) {
		loadGlsl();
	}

	im::Text( "buffers: " );
	im::SameLine();
	if( im::Button( "clear" ) ) {
		clear();
	}
	im::SameLine();
	if( im::Button( "configure" ) ) {
		markBuffersNeedConfigure();
	}

	if( mBatchComposite ) {
		auto glsl = mBatchComposite->getGlslProg();

		static float mExposure = 6.0; // TODO: add config param
		im::DragFloat( "exposure", &mExposure, 0.02f, -1, 20 ); // if < 0, disables tone-mapping

		glsl->uniform( "uExposure", mExposure );
	}

	// anti-aliasing
	if( im::CollapsingHeader( "Anti-Aliasing" ) ) {
		static vector<string> types = { "none", "msaa", "fxaa", "smaa" };
		int                   t = (int)mOptions.mAntiAliasType;
		if( im::Combo( "anti-alias", &t, types ) ) {
			setAntiAliasType( (AntiAliasType)t );
		}

		if( mOptions.mAntiAliasType == AntiAliasType::MSAA ) {
			int msaaSamples = mOptions.mMSAASamples;
			if( im::InputInt( "msaa samples", &msaaSamples ) ) {
				if( msaaSamples > mOptions.mMSAASamples ) {
					// next power of 2
					msaaSamples = nextPowerOf2( msaaSamples );
				}
				else {
					// previous power of 2
					msaaSamples = nextPowerOf2( msaaSamples / 2 );
				}
				msaaSamples = glm::clamp<int>( msaaSamples, 2, 16 );

				if( mOptions.mAntiAliasType == AntiAliasType::MSAA ) {
					// reconfigure if samples changed
					if( msaaSamples != mOptions.mMSAASamples ) {
						markBuffersNeedConfigure();
					}
				}

				mOptions.mMSAASamples = msaaSamples;
			}
		}
		else if( mOptions.mAntiAliasType == AntiAliasType::FXAA && mFXAA ) {
			mFXAA->updateUI();
		}
		else if( mOptions.mAntiAliasType == AntiAliasType::SMAA && mSMAA ) {
			mSMAA->updateUI();
		}
	}

	if( mOptions.mDebugBuffer ) {
		if( im::CollapsingHeader( "Debug Pixel", ImGuiTreeNodeFlags_DefaultOpen ) ) {
			static ivec2 pixel;
			static bool  useMouse = true;
			im::Checkbox( "use mouse", &useMouse );
			if( useMouse ) {
				// TODO: this probably won't work when rendering to WindowManager's virtual canvas. Need to think about that..
				pixel = app::getWindow()->toPoints( app::AppBase::get()->getMousePos() );
				pixel -= app::getWindow()->toPoints( app::getWindow()->getPos() );
				pixel -= ivec2( destRect.getUpperLeft() );
				pixel = clamp( pixel, ivec2( 0 ), mSize );
				im::Text( "debug pixel: %d, %d", pixel.x, pixel.y );
			}
			else {
				im::DragInt2( "pixel", &pixel.x, 1 );
				pixel = clamp( pixel, ivec2( 0 ), mFboScene->getSize() );
			}

			vec4 pixelValue;
			{
				CI_PROFILE( "getDebugPixel" );
				pixelValue = getDebugPixel( pixel );
			}

			im::DragFloat4( "debug value", &pixelValue.x );
		}
	}

	if( im::CollapsingHeader( "Bloom" ) ) {
		if( im::Checkbox( "enabled##bloom", &mOptions.mBloom ) ) {
			if( mOptions.mBloom ) {
				markBuffersNeedConfigure();
			}
			else {
				// not doing a full reconfigure, just remove the post effect
				mBloom = nullptr;
				mOptions.mGlowBuffer = false;
			}

			loadGlsl();
		}
		if( mBloom ) {
			imx::BeginDisabled( true ); // FIXME: see comment in preDraw()
			if( im::InputFloat( "downsample factor", &mOptions.mBloomDownsampleFactor, 1 ) ) {
				mOptions.mBloomDownsampleFactor = glm::clamp( mOptions.mBloomDownsampleFactor, 1.0f, 10.0f );
				markBuffersNeedConfigure();
			}
			imx::EndDisabled();
			//imx::TexturePreview( "final", mBloom->getTexture(), imageBounds );
		}
		else {
			if( ! mOptions.mBloom ) {
				im::Text( "Bloom disabled" );
			}
			else if( !mBloom ) {
				im::Text( "null mBloom" );
			}
		}
	}

	if( im::CollapsingHeader( "Motion Blur" ) ) {
		if( im::Checkbox( "enabled##motionblur", &mOptions.mMotionBlur ) ) {
			if( mOptions.mMotionBlur ) {
				markBuffersNeedConfigure();
			}
			else {
				// not doing a full reconfigure, just remove the post effect
				mMotionBlur = nullptr;

				if( mOptions.mAntiAliasType != AntiAliasType::SMAA_T2x ) {
					mOptions.mVelocityBuffer = false;
				}
			}
		}
		if( mMotionBlur ) {
			mMotionBlur->updateUI();
		}
		else {
			if( ! mOptions.mMotionBlur ) {
				im::Text( "disabled" );
			}
			else if( ! mMotionBlur ) {
				im::TextColored( Color( 1, 0, 0 ), "null mMotionBlur" );
			}
		}
	}

	if( im::CollapsingHeader( "Depth of Field" ) ) {
		if( im::Checkbox( "enabled##dof", &mOptions.mDepthOfField ) ) {
			if( mOptions.mDepthOfField ) {
				markBuffersNeedConfigure();
			}
			else {
				// not doing a full reconfigure, just remove the post effect
				mDepthOfField = nullptr;
			}
		}

		if( mDepthOfField ) {
			mDepthOfField->updateUI();
		}
		else {
			if( ! mOptions.mDepthOfField ) {
				im::Text( "disabled" );
			}
			else if( ! mDepthOfField ) {
				im::TextColored( Color( 1, 0, 0 ), "null mDepthOfField" );
			}
		}
	}

#if SCENE_GODRAYS_ENABLED
	if( im::CollapsingHeader( "Sun Rays", flags ) ) {
		if( im::Checkbox( "enabled##sunrays", &mOptions.mSunRays ) ) {
			setSunRaysEnabled( mOptions.mSunRays );
		}
		if( mSunRays ) {
			// TODO: give SunRays it's own load() / save() methods
			if( im::Checkbox( "use camera", &mOptions.mSunFromCamera ) ) {
				mSunRays->enableSunFromCamera( mOptions.mSunFromCamera );
			}
			if( im::DragFloat2( "sun pos", &mOptions.mSunPos.x, 0.001f, -1, 1 ) ) {
				mSunRays->setSunPosition( mOptions.mSunPos );
				mSunRays->enableSunFromCamera( false );
				mOptions.mSunFromCamera = false;
			}
			if( im::SliderFloat( "Power", &mOptions.mSunPower, 1, 100, "%.3f", 2 ) ) {
				mSunRays->setSunPower( mOptions.mSunPower );
			}
			if( im::SliderFloat( "Boost", &mOptions.mSunBoost, 1, 100, "%.3f", 2 ) ) {
				mSunRays->setSunBoost( mOptions.mSunBoost );
			}

			// imx::TexturePreview( "final", mSunRays->getTexture(), imageBounds );
		}
		else {
			if( !mOptions.mSunRays ) {
				im::Text( "Sun rays disabled" );
			}
			else if( !mSunRays ) {
				im::Text( "null mSunRays" );
			}
		}
	}
#endif

	if( im::CollapsingHeader( "Scene Buffers" ) ) {
		auto opts = imx::TextureViewerOptions().treeNodeFlags( ImGuiTreeNodeFlags_DefaultOpen );
		if( mFboScene ) {
			imx::Texture2d( "color", mFboScene->getColorTexture(), opts );

			if( mOptions.mDepthSource == DepthSource::Z_BUFFER ) {
				imx::TextureDepth( "depth", mFboScene->getDepthTexture(), opts );
			}

#if SCENE_GLOW_ENABLED
			if( mOptions.mGlowBuffer ) {
				imx::Texture2d( "glow", getTexture( COLOR_ATTACHMENT_GLOW ), opts );
			}
#endif

			if( mOptions.mVelocityBuffer ) {
				imx::TextureVelocity( "velocity", getTexture( COLOR_ATTACHMENT_VELOCITY ), opts );
			}
		}
		else {
			im::Text( "null mFboScene" );
		}

#if SCENE_GODRAYS_ENABLED
		// TODO: move these buffers into SunraysEffect
		// - only going to show main attachments in this window
		if( mSunRays ) {
			imx::TexturePreview( "SunRays Composite", mSunRays->getTexture(), imageBounds );
			imx::TexturePreview( "SunRays Effect", mSunRays->getTextureEffect(), imageBounds );
		}
#endif

		if( mFboComposite ) {
			imx::Texture2d( "composite", mFboComposite->getColorTexture() );
		}
		else {
			im::Text( "null mFboComposite" );
		}
	}

	im::End();
}

void PostProcess::enabledUI()
{
	im::ScopedId idScope( "scene::PostProcess" );

	im::Text( "PostProcess:" );	im::SameLine();
	im::SetCursorPosX( 120 );
	im::Checkbox( "enabled", &mOptions.mEnabled );		im::SameLine();
	im::Checkbox( "ui", &mOptions.mUIEnabled );
}

} // namespace mason::scene
