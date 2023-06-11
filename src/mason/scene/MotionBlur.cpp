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

// Adapted from G3D
// - shaders: G3D10/data-files/shader/MotionBlur/MotionBlur_gather
// - cpp: G3D10/G3D-app.lib/source/MotionBlur.cpp
// - paper: A Reconstruction Filter for Plausible Motion Blur.
//          McGuire, Hennessy, Bukowski, and Osman, I3D 2012
//          http://graphics.cs.williams.edu/papers/MotionBlurI3D12/

#include "mason/scene/MotionBlur.h"
#include "mason/scene/PostProcess.h"
#include "mason/Assets.h"
#include "mason/Common.h"
#include "mason/Profiling.h"

#include "mason/imx/ImGuiStuff.h"
#include "mason/imx/ImGuiTexture.h"

#include "cinder/gl/gl.h"
#include "cinder/Log.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason::scene {

MotionBlurEffect::MotionBlurEffect( PostProcess *postProcess )
	: PostEffect( postProcess )
{
	bool depthInColorAlphaChannel = mPostProcess->getDepthSource() == DepthSource::COLOR_ALPHA_CHANNEL;

	mConnections += ma::assets()->getShader( "mason/post/motionBlur/passthrough.vert", "mason/post/motionBlur/tileMinMax.frag",
		gl::GlslProg::Format().define( "INPUT_HAS_MIN_SPEED 0" ).label( "MotionBlur_tileMinMax (H)" ),
		[this]( gl::GlslProgRef glsl ) {
			mGlslTileMinMaxHorizontal = glsl;
		}
	);

	mConnections += ma::assets()->getShader( "mason/post/motionBlur/passthrough.vert", "mason/post/motionBlur/tileMinMax.frag",
		gl::GlslProg::Format().define( "INPUT_HAS_MIN_SPEED 1" ).label( "MotionBlur_tileMinMax" ),
		[this]( gl::GlslProgRef glsl ) {
			mGlslTileMinMax = glsl;
		}
	);

	mConnections += ma::assets()->getShader( "mason/post/motionBlur/passthrough.vert", "mason/post/motionBlur/neighborMinMax.frag",
		gl::GlslProg::Format().label( "MotionBlur_neighborMinMax" ),
		[this]( gl::GlslProgRef glsl ) {
			mGlslNeighborMinMax = glsl;
		}
	);

	mConnections += ma::assets()->getShader( "mason/post/motionBlur/passthrough.vert", "mason/post/motionBlur/gather.frag",
		gl::GlslProg::Format().define( "DEPTH_IN_COLOR_ALPHA_CHANNEL", to_string( depthInColorAlphaChannel ) ).label( "MotionBlur_gather" ),
		[this]( gl::GlslProgRef glsl ) {
			glsl->uniform( "uSamples", mNumSamples ); // TODO: remove if not used
			mGlslReconstruct = glsl;
		}
	);
}

void MotionBlurEffect::setNumSamples( int samples )
{ 
	mNumSamples = nextOdd( samples );
}

namespace {

// TODO: figure out what to do about this in PostProcess, or remove
// - g3d MotionBlurUpdate uses a different size for the cached color buffer than the input color buffer to apply()
ivec2 trimBandThickness = ivec2( 0 ); // ivec2( 64 );

}

#define TEX_INTERNAL_FORMAT GL_RGB16F
//#define TEX_INTERNAL_FORMAT GL_RGB32F

void MotionBlurEffect::updateBuffers( const ivec2 &size, int maxBlurRadiusPixels )
{
	const int w = size.x - trimBandThickness.x * 2;
	const int h = size.y - trimBandThickness.y * 2;

	// Tile boundaries will appear if the tiles are not radius x radius
	const ivec2 smallSize  = ivec2( ceil( w / (float)maxBlurRadiusPixels ), ceil( h / (float)maxBlurRadiusPixels ) );
	auto texFormat = gl::Texture::Format().internalFormat( TEX_INTERNAL_FORMAT );

	// TODO: these settings make the tex format match g3d's, but probably don't want repeat on here.
	texFormat.wrap( GL_REPEAT );
	texFormat.magFilter( GL_NEAREST );
	texFormat.minFilter( GL_NEAREST );

	if( ! mFboTileMinMax || mFboTileMinMax->getSize() != smallSize ) {
		CI_LOG_I( "creating interim Fbos of size: " << smallSize );
		{
			auto texTileMinMaxTemp = gl::Texture2d::create( h, smallSize.x, texFormat );
			texTileMinMaxTemp->setLabel( "MotionBlur_tileMinMaxTemp" );
			mFboTileMinMaxTemp = gl::Fbo::create( texTileMinMaxTemp->getWidth(), texTileMinMaxTemp->getHeight(), gl::Fbo::Format().attachment( GL_COLOR_ATTACHMENT0, texTileMinMaxTemp ).disableDepth() );
		}
		{
			auto texTileMinMax = gl::Texture2d::create( smallSize.x, smallSize.y, texFormat );
			texTileMinMax->setLabel( "MotionBlur_tileMinMax" );
			mFboTileMinMax = gl::Fbo::create( texTileMinMax->getWidth(), texTileMinMax->getHeight(), gl::Fbo::Format().attachment( GL_COLOR_ATTACHMENT0, texTileMinMax ).disableDepth() );
		}
		{
			auto texNeighborMinMax = gl::Texture2d::create( smallSize.x, smallSize.y, texFormat );
			texNeighborMinMax->setLabel( "MotionBlur_neighborMinMax" );
			mFboNeighborMinMax = gl::Fbo::create( texNeighborMinMax->getWidth(), texNeighborMinMax->getHeight(), gl::Fbo::Format().attachment( GL_COLOR_ATTACHMENT0, texNeighborMinMax ).disableDepth() );
		}
	}

	// TODO: rename FboReconstruct -> FboGather for consistency with G3D (review their arrangment again)
	// - this name came from the previous impl (wicks)
	if( ! mFboReconstruct || mFboReconstruct->getSize() != ivec2( w, h ) ) {
		CI_LOG_I( "creating mFboReconstruct of size: " << ivec2( w, h ) );
		auto texReconstruct	= gl::Texture2d::create( w, h, texFormat );
		texReconstruct->setLabel( "MotionBlur_reconstruct" );
		mFboReconstruct = gl::Fbo::create( texReconstruct->getWidth(), texReconstruct->getHeight(), gl::Fbo::Format().attachment( GL_COLOR_ATTACHMENT0, texReconstruct ).disableDepth() );
	}
}

void MotionBlurEffect::process( const ci::gl::FboRef &source )
{
	CI_ASSERT( mNumSamples % 2 == 1 );
	auto velocityTex = mPostProcess->getTexture( PostProcess::COLOR_ATTACHMENT_VELOCITY );
	CI_ASSERT( velocityTex );
	if( ! velocityTex )
		return;

	MA_PROFILE( "MotionBlur process" );

	const ivec2 size = mPostProcess->getSize();
	const int dimension = size.x < size.y ? size.x : size.y;
	const int maxBlurRadiusPixels     = max( 4, (int)ceil( float( dimension ) * mMaxBlurDiameterFraction / 2.0f ) );


	updateBuffers( size, maxBlurRadiusPixels );

	// TileMax: Each tile stores the dominant (i.e. highest magnitude) velocity for all the original values within that tile.
	// horizontal pass
	if( mGlslTileMinMaxHorizontal ) {
		gl::ScopedFramebuffer scopedFbo( mFboTileMinMaxTemp );
		gl::ScopedViewport scopedViewport( ivec2( 0, 0 ), mFboTileMinMaxTemp->getSize() );
		gl::ScopedMatrices	matrices;
		gl::setMatricesWindow( mFboTileMinMaxTemp->getSize() );

		mGlslTileMinMaxHorizontal->uniform( "maxBlurRadius", (int)maxBlurRadiusPixels );
		mGlslTileMinMaxHorizontal->uniform( "inputShift", vec2( trimBandThickness ) );

		gl::ScopedTextureBind scopedTex( velocityTex );
		gl::ScopedGlslProg prog( mGlslTileMinMaxHorizontal );

		gl::drawSolidRect( mFboTileMinMaxTemp->getBounds() );
	}
	// vertical pass
	// TODO: simplify these passes into a base method such as renderToBuffer( fbo, glsl );
	// - not sure yet if that will work for the reconstruct but will try
	if( mGlslTileMinMax ) {
		gl::ScopedFramebuffer scopedFbo( mFboTileMinMax );
		gl::ScopedViewport scopedViewport( ivec2( 0, 0 ), mFboTileMinMax->getSize() );
		gl::ScopedMatrices	matrices;
		gl::setMatricesWindow( mFboTileMinMax->getSize() );

		mGlslTileMinMax->uniform( "maxBlurRadius", (int)maxBlurRadiusPixels );
		mGlslTileMinMax->uniform( "inputShift", vec2( 0 ) );

		gl::ScopedTextureBind scopedTex( mFboTileMinMaxTemp->getColorTexture() );
		gl::ScopedGlslProg prog( mGlslTileMinMax );

		gl::drawSolidRect( mFboTileMinMax->getBounds() );
	}

	// NeighborMax: Each tile's dominant half-velocity is compared against its neighbors', and it stores the highest
	// dominant velocity found. This effectively "smears" the highest velocities onto other neighboring tiles
	if( mGlslNeighborMinMax ) {
		gl::ScopedFramebuffer scopedFbo( mFboNeighborMinMax );
		gl::ScopedViewport scopedViewport( ivec2( 0, 0 ), mFboNeighborMinMax->getSize() );
		gl::ScopedMatrices	matrices;
		gl::setMatricesWindow( mFboNeighborMinMax->getSize() );

		gl::ScopedTextureBind scopedTex( mFboTileMinMax->getColorTexture() );
		gl::ScopedGlslProg prog( mGlslNeighborMinMax );

		gl::drawSolidRect( mFboTileMinMax->getBounds() );
	}

	// Reconstruct
	if( mGlslReconstruct ) {
		gl::ScopedFramebuffer scopedFbo( mFboReconstruct );
		gl::ScopedViewport scopedViewport( ivec2( 0, 0 ), mFboReconstruct->getSize() );
		gl::ScopedMatrices	matrices;
		gl::setMatricesWindow( mFboReconstruct->getSize() );

		gl::ScopedTextureBind scopedTex0( source->getColorTexture(), 0 );
		gl::ScopedTextureBind scopedTex1( velocityTex, 1 );

		gl::ScopedTextureBind scopedTex2( mFboNeighborMinMax->getColorTexture(), 2 );

		gl::TextureRef depthTex = mPostProcess->getDepthTexture();
		if( depthTex ) {
			gl::context()->pushTextureBinding( depthTex->getTarget(), depthTex->getId(), 3 );
		}
		else {
			// require that depth is provided via color alpha channel
			CI_ASSERT( mPostProcess->getDepthSource() == DepthSource::COLOR_ALPHA_CHANNEL );
		}

		gl::ScopedGlslProg scopedGlsl( mGlslReconstruct );
		
		mGlslReconstruct->uniform( "uColorMap", 0 );
		mGlslReconstruct->uniform( "uVelocityMap", 1 );
		mGlslReconstruct->uniform( "uNeighborMinMaxMap", 2 );
		mGlslReconstruct->uniform( "uTexDepth", 3 );
		mGlslReconstruct->uniform( "uVelocityScale", mVelocityScale );
		mGlslReconstruct->uniform( "maxBlurRadius", maxBlurRadiusPixels );
		mGlslReconstruct->uniform( "numSamplesOdd", mNumSamples );
		mGlslReconstruct->uniform( "trimBandThickness", trimBandThickness );
		mGlslReconstruct->uniform( "exposureTime", mExposureTimeFraction );

		gl::drawSolidRect( source->getBounds() );

		if( depthTex ) {
			auto depthTex = source->getDepthTexture();
			gl::context()->popTextureBinding( depthTex->getTarget(), 3 );
		}
	}
}

void MotionBlurEffect::updateUI()
{
	int samples = mNumSamples;
	if( im::DragInt( "samples", &samples, 0.05f, 1, 999 ) ) {
		// ensure always odd
		if( samples % 2 == 0 ) {
			mNumSamples = samples > mNumSamples ? samples + 1 : samples - 1;
		}
	}

	// note: changing max blur will cause the buffers to be resized
	im::DragFloat( "max diameter", &mMaxBlurDiameterFraction, 0.01f, 0, 3 );
	// in fraction of frame duration
	im::DragFloat( "exposure", &mExposureTimeFraction, 0.01f, 0, 3 );	 
	im::DragFloat( "velocity scale", &mVelocityScale, 0.01f, 0, 3 );

	if( im::CollapsingHeader( "buffers" ) ) {
		imx::TextureViewerOptions opts;
		opts.mTreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
		if( mFboTileMinMax ) {
			imx::Texture2d( "tileMinMaxTemp", mFboTileMinMaxTemp->getColorTexture(), opts );		
			imx::Texture2d( "tileMinMax", mFboTileMinMax->getColorTexture(), opts );
			imx::Texture2d( "neighborMinMax", mFboNeighborMinMax->getColorTexture(), opts );
		}
		if( mFboReconstruct ) {
			imx::Texture2d( "reconstruct", mFboReconstruct->getColorTexture(), opts );
		}
	}
}

} // namespace mason::scene