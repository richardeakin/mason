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

#include "mason/scene/PostEffects.h"
#include "mason/scene/PostProcess.h"
#include "mason/Assets.h"
#include "mason/imx/ImGuiStuff.h"
#include "mason/imx/ImGuiTexture.h"
#include "mason/Common.h"
#include "mason/Profiling.h"

#include "mason/aa/smaa/AreaTex.h"
#include "mason/aa/smaa/SearchTex.h"

#include "cinder/gl/gl.h"
#include "cinder/Log.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason::scene {

// ----------------------------------------------------------------------------------------------------
// PostEffect
// ----------------------------------------------------------------------------------------------------

PostEffect::PostEffect( PostProcess *postProcess )
	: mPostProcess( postProcess )
{
}


PostEffect::~PostEffect()
{
}


// ----------------------------------------------------------------------------------------------------
// BloomEffect
// ----------------------------------------------------------------------------------------------------

BloomEffect::BloomEffect( PostProcess *postProcess, const ivec2 &size )
	: PostEffect( postProcess )
{
	// TODO: add method that returns gl color format instead (when 8 and 16-bit support is added)
	GLint internalFormat = postProcess->getColorFormat() == ColorFormat::RGBA32 ? GL_RGBA32F : GL_RGB32F;

	auto texFormat = gl::Texture::Format()
		.internalFormat( internalFormat )
		.wrap( GL_CLAMP_TO_EDGE )
	;

	auto fboFormat1 = gl::Fbo::Format().disableDepth();
	auto fboFormat2 = fboFormat1; // need a second copy as we create two Textures
	fboFormat1.attachment( GL_COLOR_ATTACHMENT0, gl::Texture2d::create( size.x, size.y, texFormat.label( "BloomEffect blur 1" ) ) );
	fboFormat2.attachment( GL_COLOR_ATTACHMENT0, gl::Texture2d::create( size.x, size.y, texFormat.label( "BloomEffect blur 2" ) ) );

	mFboBlur1 = gl::Fbo::create( size.x, size.y, fboFormat1.label( "BloomEffect blur 1" ) );
	mFboBlur2 = gl::Fbo::create( size.x, size.y, fboFormat2.label( "BloomEffect blur 1" ) );

	mConnGlsl = ma::assets()->getShader( "mason/passthrough.vert", "mason/post/blur.frag", [this]( gl::GlslProgRef glsl ) {
		glsl->uniform( "uTexColor", 0 );

		mGlslBlur = glsl;
	} );
}

void BloomEffect::process( const ci::gl::FboRef &source )
{
	if( ! mGlslBlur )
		return;

	// first pass: horizontal
	{
		gl::ScopedFramebuffer scopedFbo( mFboBlur1 );
		gl::ScopedViewport scopedViewport( 0, 0, mFboBlur1->getWidth(), mFboBlur1->getHeight() );
		gl::ScopedMatrices scopedMatrices;
		gl::setMatricesWindow( mFboBlur1->getSize() );

		gl::ScopedGlslProg glslScope( mGlslBlur );

		// blur horizontally and the size of 1 pixel
		mGlslBlur->uniform( "uSampleOffset", vec2( 1.0f / mFboBlur1->getWidth(), 0.0f ) );

# if SCENE_GLOW_ENABLED
		auto glowTex = mPostProcess->getTexture( PostProcess::COLOR_ATTACHMENT_GLOW );
		gl::ScopedTextureBind scopedTex0( glowTex, 0 );
#endif

		gl::clear();
		gl::drawSolidRect( mFboBlur1->getBounds() );
	}

	// second pass: vertical
	{
		gl::ScopedFramebuffer scopedFbo( mFboBlur2 );
		gl::ScopedViewport scopedViewport( 0, 0, mFboBlur2->getWidth(), mFboBlur2->getHeight() );
		gl::ScopedMatrices scopedMatrices;
		gl::setMatricesWindow( mFboBlur2->getSize() );

		gl::ScopedGlslProg glslScope( mGlslBlur );

		// blur vertically the size of 1 pixel
		mGlslBlur->uniform( "uSampleOffset", vec2( 0.0f, 1.0f / mFboBlur2->getHeight() ) );

		gl::ScopedTextureBind tex0( mFboBlur1->getColorTexture(), 0 );

		gl::clear();
		gl::drawSolidRect( mFboBlur2->getBounds() );
	}
}

// ----------------------------------------------------------------------------------------------------
// FXAA
// ----------------------------------------------------------------------------------------------------

FXAA::FXAA()
{
	const fs::path vertPath = "mason/aa/fxaa/fxaa.vert";
	const fs::path fragPath = "mason/aa/fxaa/fxaa.frag";
	auto rect = geom::Rect( Rectf( 0, 0, 1, 1 ) );

#if 1
	// hot-loading
	mConnGlsl = ma::assets()->getShader( vertPath, fragPath, [this, rect]( gl::GlslProgRef glsl ) {
		mBatch = gl::Batch::create( rect, glsl );
	} );
#else
	// no hot-loading
	try {
		auto glsl = gl::GlslProg::create( app::loadAsset( vertPath ), app::loadAsset( fragPath ) );
		mBatch = gl::Batch::create( rect, glsl );
	}
	catch( const std::exception &e ) {
		CI_LOG_EXCEPTION( "exception caught loading fxaa.vert / frag", e );
	}
#endif
}

void FXAA::apply( const gl::FboRef &source, const gl::FboRef &dest )
{
	gl::ScopedFramebuffer fbo( dest );
	gl::ScopedViewport viewport( 0, 0, dest->getWidth(), dest->getHeight() );
	gl::ScopedMatrices matrices;
	gl::setMatricesWindow( dest->getSize(), false );


	// Make sure our source is linearly interpolated.
	GLenum minFilter = source->getFormat().getColorTextureFormat().getMinFilter();
	GLenum magFilter = source->getFormat().getColorTextureFormat().getMagFilter();
	source->getColorTexture()->setMinFilter( GL_LINEAR );
	source->getColorTexture()->setMagFilter( GL_LINEAR );

	// Perform FXAA anti-aliasing.
	gl::clear( ColorA( 0, 0, 0, 0 ) );
	draw( source->getColorTexture(), dest->getBounds() );

	// Restore texture parameters.
	source->getColorTexture()->setMinFilter( minFilter );
	source->getColorTexture()->setMagFilter( magFilter );
}

void FXAA::draw( const gl::TextureRef &source, const ci::Rectf &destRect )
{
	if( ! mBatch )
		return;

	ivec2 texSize = source->getSize();
	vec2 destSize = destRect.getSize();

	const float w = (float)destRect.getWidth();
	const float h = (float)destRect.getHeight();

	mBatch->getGlslProg()->uniform( "uExtents", vec4( 1.0f / w, 1.0f / h, w, h ) );

	gl::ScopedTextureBind tex0( source );
	gl::ScopedModelMatrix modelScope;
	gl::translate( destRect.getUpperLeft() );
	gl::scale( destRect.getSize() );

	mBatch->draw();
}

void FXAA::updateUI()
{
	if( ! mBatch )
		return;


	// TODO: move these to member vars, add load() / save() methods
	static float mQualitySubpix = 0.75f;
	im::DragFloat( "quality subpix", &mQualitySubpix, 0.002f, 0, 1 );

	static float mQualityEdgeThreshold = 0.033f;
	im::DragFloat( "quality edge threshold", &mQualityEdgeThreshold, 0.002f, 0, 1 );

	auto glsl = mBatch->getGlslProg();
	glsl->uniform( "uQualitySubpix", mQualitySubpix );
	glsl->uniform( "uQualityEdgeThreshold", mQualityEdgeThreshold );
}

// ----------------------------------------------------------------------------------------------------
// SMAA
// ----------------------------------------------------------------------------------------------------

SMAA::SMAA()
{
	loadGlsl();

	// Create lookup textures
	gl::Texture2d::Format fmt;
	fmt.setMinFilter( GL_LINEAR );
	fmt.setMagFilter( GL_LINEAR );
	fmt.setWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );

	// Search Texture (Grayscale, 8 bits unsigned).
	fmt.setInternalFormat( GL_R8 );
	fmt.setSwizzleMask( GL_RED, GL_RED, GL_RED, GL_ONE );
	mSearchTex = gl::Texture2d::create( searchTexBytes, GL_RED, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, fmt );

	// Area Texture (Red+Green Channels, 8 bits unsigned).
	fmt.setInternalFormat( GL_RG8 );
	fmt.setSwizzleMask( GL_RED, GL_GREEN, GL_ZERO, GL_ONE );
	mAreaTex = gl::Texture2d::create( areaTexBytes, GL_RG, AREATEX_WIDTH, AREATEX_HEIGHT, fmt );

	// Specify the Fbo format.
	gl::Texture2d::Format tfmt;
	tfmt.setMinFilter( GL_LINEAR );
	tfmt.setMagFilter( GL_LINEAR );
	tfmt.setInternalFormat( GL_RGBA8 );

	mFboFormat.setColorTextureFormat( tfmt );
	mFboFormat.enableDepthBuffer( false );
}

void SMAA::createBuffers( int width, int height )
{
	// Create or resize frame buffers
	if( ! mFboEdgePass || mFboEdgePass->getWidth() != width || mFboEdgePass->getHeight() != height ) {
		mFboEdgePass = gl::Fbo::create( width, height, mFboFormat.label( "SMAA Edge Fbo" ) );
	}

	if( ! mFboBlendPass || mFboBlendPass->getWidth() != width || mFboBlendPass->getHeight() != height ) {
		mFboBlendPass = gl::Fbo::create( width, height, mFboFormat.label( "SMAA Blend Fbo" ) );
	}

	mMetrics = vec4( 1.0f / width, 1.0f / height, (float) width, (float) height );
}

void SMAA::loadGlsl()
{
	mConnections.clear();

	fs::path vertPath = "mason/aa/smaa/smaa.vert";
	fs::path fragPath = "mason/aa/smaa/smaa.frag";
	auto geom = geom::Rect( Rectf( 0, 0, 1, 1 ) );

#if 1
	// hot-loading
	// note to not use the same GlslProg::Format for the same shader with varying defines because the ShaderProcessorRef is shared among them and they will be overwritten (so not accurate on reload)
	auto fmt1 = gl::GlslProg::Format().define( presetToString( mPreset ) ).define( "SMAA_PASS", "1" ).label( "smaa_pass1" );
	mConnections += ma::assets()->getShader( vertPath, fragPath, fmt1, [this, geom]( gl::GlslProgRef glsl ) {
		CI_LOG_I( "smaa-1 loaded, glsl address: " << hex << glsl.get() << dec );
		mBatchFirstPass = gl::Batch::create( geom, glsl );
	} );
	auto fmt2 = gl::GlslProg::Format().define( presetToString( mPreset ) ).define( "SMAA_PASS", "2" ).label( "smaa_pass2" );
	mConnections += ma::assets()->getShader( vertPath, fragPath, fmt2, [this, geom]( gl::GlslProgRef glsl ) {
		CI_LOG_I( "smaa-2 loaded, glsl address: " << hex << glsl.get() << dec );
		mBatchSecondPass = gl::Batch::create( geom, glsl );
	} );
	auto fmt3 = gl::GlslProg::Format().define( presetToString( mPreset ) ).define( "SMAA_PASS", "3" ).label( "smaa_pass3" );
	mConnections += ma::assets()->getShader( vertPath, fragPath, fmt3, [this, geom]( gl::GlslProgRef glsl ) {
		CI_LOG_I( "smaa-3 loaded, glsl address: " << hex << glsl.get() << dec );
		mBatchThirdPass = gl::Batch::create( geom, glsl );
	} );
#else
	// no hot-loading
	try {
		fmt.vertex( app::loadAsset( vertPath ) ).fragment( app::loadAsset( fragPath ) );
		{
			fmt.define( "SMAA_PASS", "1" ).label( "smaa_pass1" );
			mBatchFirstPass = gl::Batch::create( geom, gl::GlslProg::create( fmt ) );
		}
		{
			fmt.define( "SMAA_PASS", "2" ).label( "smaa_pass2" );
			mBatchSecondPass = gl::Batch::create( geom, gl::GlslProg::create( fmt ) );
		}
		{
			fmt.define( "SMAA_PASS", "3" ).label( "smaa_pass3" );
			mBatchThirdPass = gl::Batch::create( geom, gl::GlslProg::create( fmt ) );
		}
		CI_LOG_I( "loaded shaders for three passes (non hot-loading)" );
	}
	catch( const exception &e ) {
		CI_LOG_EXCEPTION( "Failed to load SMAA shaders", e );
	}
#endif
}


// static
const char* SMAA::presetToString( Preset preset )
{
	switch( preset ) {
		case Preset::Low:		return "SMAA_PRESET_LOW";
		case Preset::Medium:	return "SMAA_PRESET_MEDIUM";
		case Preset::High:		return "SMAA_PRESET_HIGH";
		case Preset::Ultra:		return "SMAA_PRESET_ULTRA";
		default: CI_ASSERT_NOT_REACHABLE();
	}

	return "(unknown)";
}

void SMAA::setPreset( Preset preset )
{
	if( mPreset == preset )
		return;

	mPreset = preset;
	loadGlsl();
}

gl::TextureRef SMAA::getEdgePass() const
{
	if( ! mFboEdgePass ) {
		return nullptr;
	}

	return mFboEdgePass->getColorTexture();
}

gl::TextureRef SMAA::getBlendPass() const
{
	if( ! mFboBlendPass ) {
		return nullptr;
	}

	return mFboBlendPass->getColorTexture();
}

void SMAA::apply( const gl::FboRef &source, const gl::FboRef &dest )
{
	gl::ScopedFramebuffer fbo( dest );
	gl::ScopedViewport viewport( 0, 0, dest->getWidth(), dest->getHeight() );
	gl::ScopedMatrices matrices;
	gl::setMatricesWindow( dest->getSize() );

	// Make sure our source is linearly interpolated.
	GLenum minFilter = source->getFormat().getColorTextureFormat().getMinFilter();
	GLenum magFilter = source->getFormat().getColorTextureFormat().getMagFilter();
	bool filterChanged = ( minFilter != GL_LINEAR || magFilter != GL_LINEAR );
	if( filterChanged ) {
		CI_LOG_W( "source min or mag filter not linear, updating." );
		source->getColorTexture()->setMinFilter( GL_LINEAR );
		source->getColorTexture()->setMagFilter( GL_LINEAR );
	}

	// Perform SMAA anti-aliasing.
	gl::clear( ColorA( 0, 0, 0, 0 ) );
	draw( source->getColorTexture(), dest->getBounds() );

	// Restore texture parameters.
	if( filterChanged ) {
		source->getColorTexture()->setMinFilter( minFilter );
		source->getColorTexture()->setMagFilter( magFilter );
	}
}

// TODO: use a Rectf for bounds - it is used with translation below
void SMAA::draw( const gl::Texture2dRef &source, const Area &bounds )
{
	if( ! mBatchFirstPass || ! mBatchSecondPass || ! mBatchThirdPass )
		return;

	// Create or resize buffers.
	const int width = source->getWidth();
	const int height = source->getHeight();
	createBuffers( width, height );

	// Apply first two passes.
	doEdgePass( source );
	doBlendPass();

	// Apply SMAA.
	{
		gl::ScopedTextureBind tex0( source );
		gl::ScopedTextureBind tex1( mFboBlendPass->getColorTexture(), 1 );
		mBatchThirdPass->getGlslProg()->uniform( "SMAA_RT_METRICS", mMetrics );
		mBatchThirdPass->getGlslProg()->uniform( "uColorTex", 0 );
		mBatchThirdPass->getGlslProg()->uniform( "uBlendTex", 1 );

		gl::ScopedModelMatrix modelScope;
		gl::translate( bounds.getUL() );
		gl::scale( bounds.getWidth(), bounds.getHeight(), 1.0f );

		mBatchThirdPass->draw();
	}
}

void SMAA::doEdgePass( const gl::Texture2dRef &source )
{
	// Enable frame buffer, bind textures and shader.
	gl::ScopedFramebuffer fbo( mFboEdgePass );

	gl::ScopedViewport viewportScope( 0, 0, mFboEdgePass->getWidth(), mFboEdgePass->getHeight() );

	gl::ScopedMatrices matScope;
	gl::setMatricesWindow( mFboEdgePass->getSize() );

	gl::clear( ColorA( 0, 0, 0, 0 ) );

	gl::ScopedTextureBind tex0( source );
	mBatchFirstPass->getGlslProg()->uniform( "SMAA_RT_METRICS", mMetrics );
	mBatchFirstPass->getGlslProg()->uniform( "uColorTex", 0 );

	gl::ScopedModelMatrix modelScope;
	gl::scale( mFboEdgePass->getWidth(), mFboEdgePass->getHeight(), 1.0f );

	mBatchFirstPass->draw();
}

void SMAA::doBlendPass()
{
	gl::ScopedFramebuffer fbo( mFboBlendPass );
	gl::clear( ColorA( 0, 0, 0, 0 ) );

	gl::ScopedTextureBind tex0( mFboEdgePass->getColorTexture() );
	gl::ScopedTextureBind tex1( mAreaTex, 1 );
	gl::ScopedTextureBind tex2( mSearchTex, 2 );
	mBatchSecondPass->getGlslProg()->uniform( "SMAA_RT_METRICS", mMetrics );
	mBatchSecondPass->getGlslProg()->uniform( "uEdgesTex", 0 );
	mBatchSecondPass->getGlslProg()->uniform( "uAreaTex", 1 );
	mBatchSecondPass->getGlslProg()->uniform( "uSearchTex", 2 );

	gl::ScopedModelMatrix modelScope;
	gl::scale( mFboBlendPass->getWidth(), mFboBlendPass->getHeight(), 1.0f );

	mBatchSecondPass->draw();
}


void SMAA::updateUI()
{
	if( im::Button( "reload shader" ) ) {
		loadGlsl();
	}

	static vector<string> presets = { "low", "medium", "high", "ultra" };
	int t = (int)mPreset;
	if( im::Combo( "preset", &t, presets ) ) {
		setPreset( (Preset)t );
	}

	if( im::CollapsingHeader( "buffers" ) ) {
		imx::TextureViewerOptions opts;
		opts.mTreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;

		imx::Texture2d( "Edge Pass", getEdgePass(), opts );
		imx::Texture2d( "Blend Pass", getBlendPass(), opts );

		if( im::Button( "write to file" ) ) {
			writeImage( "smaa_edgePass.png", getEdgePass()->createSource() );
			writeImage( "smaa_blendPass.png", getEdgePass()->createSource() );
		}
	}
}

} // namespace mason::scene
