/*
Copyright (c) 2019, Richard Eakin project - All rights reserved.

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

#include "mason/scene/DepthOfField.h"
#include "mason/scene/PostProcess.h"
#include "mason/Assets.h"

#include "mason/extra/ImGuiStuff.h"
#include "mason/extra/ImGuiTexture.h"
#include "mason/Profiling.h"

#include "cinder/Log.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason::scene {

namespace {

const float kFilmHeight = 0.024f;

const char* kernelSizeToString( DepthOfFieldBokehEffect::KernelSize size )
{
	switch( size ) {
		case DepthOfFieldBokehEffect::KernelSize::SMALL:		return "KERNEL_SMALL";
		case DepthOfFieldBokehEffect::KernelSize::MEDIUM:		return "KERNEL_MEDIUM";
		case DepthOfFieldBokehEffect::KernelSize::LARGE:		return "KERNEL_LARGE";
		case DepthOfFieldBokehEffect::KernelSize::VERY_LARGE:	return "KERNEL_VERYLARGE";
		default:	CI_ASSERT_NOT_REACHABLE();
	}

	return "(unknown)";
}

} // anonymous namespace

DepthOfFieldBokehEffect::DepthOfFieldBokehEffect( PostProcess *postProcess )
	: PostEffect( postProcess )

{
	loadShaders(); // TODO: move to load() for config saving
}

DepthOfFieldBokehEffect::~DepthOfFieldBokehEffect()
{
}

void DepthOfFieldBokehEffect::loadShaders()
{
	mConnections.clear();

	mConnections += assets()->getShader( "mason/passthrough.vert", "mason/post/depthOfFieldBokeh/bokeh1_prefilter.frag",
		[this]( gl::GlslProgRef glsl ) {
			mGlslPrefilter = glsl;
		}
	);
	mConnections += assets()->getShader( "mason/passthrough.vert", "mason/post/depthOfFieldBokeh/bokeh2_diskblur.frag",
		gl::GlslProg::Format().define( kernelSizeToString( mKernelSize ) ),
		[this]( gl::GlslProgRef glsl ) {
			mGlslDiskBlur = glsl;
		}
	);
	mConnections += assets()->getShader( "mason/passthrough.vert", "mason/post/depthOfFieldBokeh/bokeh3_tentfiler.frag",
		[this]( gl::GlslProgRef glsl ) {
			mGlslTentFilter = glsl;
		}
	);
	mConnections += assets()->getShader( "mason/passthrough.vert", "mason/post/depthOfFieldBokeh/bokeh4_composition.frag",
		[this]( gl::GlslProgRef glsl ) {
			mGlslComposition = glsl;
		}
	);
}

void DepthOfFieldBokehEffect::initBuffers( const ci::ivec2 &size )
{
	CI_LOG_I( "size: " << size );

	auto halfRes = size / 2;
	auto texFmt = gl::Texture2d::Format()
		.internalFormat( GL_RGBA16F )
		.dataType( GL_HALF_FLOAT )
		.magFilter( GL_LINEAR )
		.minFilter( GL_LINEAR )
		.wrap( GL_CLAMP_TO_EDGE )
		.mipmap( false )
	;

	if( !mFbo1 || halfRes != mFbo1->getSize() ) {
		auto fmt = gl::Fbo::Format().label( "FboBokeh1" ).colorTexture( texFmt ).disableDepth();
		mFbo1 = gl::Fbo::create( halfRes.x, halfRes.y, fmt );
	}
	auto fmt = gl::Fbo::Format().label( "FboBokeh2" ).colorTexture( texFmt ).disableDepth();
	if( !mFbo2 || halfRes != mFbo2->getSize() ) {
		mFbo2 = gl::Fbo::create( halfRes.x, halfRes.y, fmt );
	}

	if( ! mBaseFbo || size != mBaseFbo->getSize() ) {
		auto texFmt = gl::Texture2d::Format()
			.internalFormat( GL_RGBA16F )
			.dataType( GL_HALF_FLOAT )
			.magFilter( GL_NEAREST )
			.minFilter( GL_NEAREST )
			.wrap( GL_CLAMP_TO_EDGE )
			.mipmap( false );
		auto fmt = gl::Fbo::Format().label( "FboBokehMain" );
		fmt.setColorTextureFormat( texFmt );
		fmt.disableDepth();
		mBaseFbo = gl::Fbo::create( size.x, size.y, fmt );
	}
}

void DepthOfFieldBokehEffect::process( const ci::gl::FboRef &source )
{
	if( ! mIsActive )
		return;

	if( ! mBaseFbo || mBaseFbo->getSize() != source->getSize() ) {
		initBuffers( source->getSize() );
	}

	float aspectRatio = (float)mFbo1->getHeight() / (float)mFbo1->getWidth();
	float s1 = mFocusDistance;
	float f = mFocalLength / 1000.0f;
	s1 = glm::max( s1, f );
	float coeff = f * f / ( mFStop * ( s1 - f ) * kFilmHeight * 2.0f );
	float radiusInPixels = float( mKernelSize ) * 4 + 6.0f;

	//float maxCoC = glm::min( 0.05f, radiusInPixels / (float) mFg->getResource( mInputs.at(0) )->getHeight() );
	float maxCoC = glm::min( 0.05f, radiusInPixels / (float) source->getHeight() );

	//1st pass - downres & prefilter
	{
		mGlslPrefilter->uniform( "uDistance", s1 );
		mGlslPrefilter->uniform( "uLensCoeff", coeff );
		mGlslPrefilter->uniform( "uMaxCoC", maxCoC );
		mGlslPrefilter->uniform( "uRcpMaxCoc", 1.0f / maxCoC );
		mGlslPrefilter->uniform( "uCameraRange", vec2( mPostProcess->getCamera().getNearClip(),  mPostProcess->getCamera().getFarClip() ) );

		gl::ScopedFramebuffer pushFbo{ mFbo1 };
		gl::ScopedViewport viewport( 0, 0, mFbo1->getWidth(), mFbo1->getHeight() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( mFbo1->getSize() );
		gl::clear();

		//blit( mFbo1->getSize(), mInputs, mGlslPrefilter );

		gl::ScopedGlslProg scopedGlsl( mGlslPrefilter );
		gl::ScopedTextureBind scopedTex0( source->getColorTexture(), 0 );
		gl::ScopedTextureBind scopedTex1( mPostProcess->getDepthTexture(), 1 );

		gl::drawSolidRect( mFbo1->getBounds() );
	}

	//2nd pass - disk blur
	{
		gl::ScopedFramebuffer pushFbo{ mFbo2 };
		gl::ScopedViewport viewport( 0, 0, mFbo2->getWidth(), mFbo2->getHeight() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( mFbo2->getSize() );
		gl::clear();

		mGlslDiskBlur->uniform( "uRcpAspect", aspectRatio );
		mGlslDiskBlur->uniform( "uMaxCoC", maxCoC );

		//blit( mFbo2->getSize(), { mFbo1->getColorTexture() }, mGlslDiskBlur );

		gl::ScopedGlslProg scopedGlsl( mGlslDiskBlur );
		gl::ScopedTextureBind scopedTex( mFbo1->getColorTexture(), 0 );

		gl::drawSolidRect( mFbo2->getBounds() );
	}

	//3rd pass - Tent filter
	{
		gl::ScopedFramebuffer pushFbo{ mFbo1 };
		gl::ScopedViewport viewport( 0, 0, mFbo1->getWidth(), mFbo1->getHeight() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( mFbo1->getSize() );
		gl::clear();

		//blit( mFbo1->getSize(), { mFbo2->getColorTexture() }, mGlslTentFilter );

		gl::ScopedGlslProg scopedGlsl( mGlslTentFilter );
		gl::ScopedTextureBind scopedTex( mFbo2->getColorTexture(), 0 );

		gl::drawSolidRect( mFbo1->getBounds() );
	}

	//4th pass - Composition
	{
		gl::ScopedFramebuffer pushFbo{ mBaseFbo };
		gl::ScopedViewport viewport( 0, 0, mBaseFbo->getWidth(), mBaseFbo->getHeight() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( mBaseFbo->getSize() );
		gl::clear();

		//blit( mBaseFbo->getSize(), { mFg->getResource( mInputs.at( 0 ) ), mFbo1->getColorTexture() }, mGlslComposition );

		gl::ScopedGlslProg scopedGlsl( mGlslComposition );
		gl::ScopedTextureBind scopedTex0( source->getColorTexture(), 0 );
		gl::ScopedTextureBind scopedTex1( mFbo1->getColorTexture(), 1 ); // TODO: check this

		gl::drawSolidRect( mBaseFbo->getBounds() );
	}
}

void DepthOfFieldBokehEffect::updateUI()
{
	if( im::Checkbox( "Active", &mIsActive ) ) {
		//if( mIsActive ) {
		//	mFg->setResource( getOutput( 0 ), mBaseFbo->getColorTexture() );
		//}
		//else{
		//	mFg->setResource( getOutput( 0 ), mFg->getResource( mInputs.at( 0 ) ) );
		//}

	}

	im::DragFloat( "Focus Distance", &mFocusDistance, 0.05f, 0.1f, 100.0f );
	im::DragFloat( "Aperture (f-stop)", &mFStop, 0.01f, 1.0f, 20.0f );
	im::DragFloat( "Focal Length (mm)", &mFocalLength, 1.0f, 10.0f, 300.0f );

	if( im::CollapsingHeader( "buffers" ) ) {
		imx::TextureViewerOptions opts;
		opts.mTreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
		if( mFbo1 ) {
			imx::Texture2d( "fbo1", mFbo1->getColorTexture(), opts );
		}
		else {
			im::Text( "null mFbo1" );
		}
		if( mFbo2 ) {
			imx::Texture2d( "fbo2", mFbo2->getColorTexture(), opts );
		}
		else {
			im::Text( "null mFbo2" );
		}
		if( mBaseFbo ) {
			imx::Texture2d( "base", mBaseFbo->getColorTexture(), opts );
		}
		else {
			im::Text( "null mBaseFbo" );
		}
	}
}

} // namespace mason::scene