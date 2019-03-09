/*
Copyright (c) 2018, Richard Eakin - All rights reserved.

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

#include "mason/RenderToTexture.h"
#include "cinder/Log.h"

#include "cinder/gl/Context.h"
#include "cinder/gl/scoped.h"

using namespace ci;
using namespace std;

namespace mason {

RenderToTexture::RenderToTexture( const ci::ivec2 &size, const Format &format )
	: mFormat( format ), mSignalDraw( new signals::Signal<void ()> )
{
	if( size.x > 0 && size.y > 0 ) {
		setSize( size );
	}
}

void RenderToTexture::setSize( const ci::ivec2 &size )
{
	ivec2 clampedSize = ivec2( max<int>( 1, size.x ), max<int>( 1, size.y ) );

	if( mFbo && mFbo->getSize() == clampedSize )
		return;

	auto fboFormat = gl::Fbo::Format();
	fboFormat.colorTexture(
		gl::Texture2d::Format()
		.internalFormat( GL_RGBA )
		.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		.loadTopDown( mFormat.mLoadTopDown )
	);
	fboFormat.samples( mFormat.mMsaaSamples );

	mFbo = gl::Fbo::create( clampedSize.x, clampedSize.y, fboFormat );
}

ci::ivec2 RenderToTexture::getSize() const
{ 
	if( ! mFbo )
		return { 0, 0 };

	return mFbo->getSize();
}

ci::gl::Texture2dRef RenderToTexture::getTexture() const
{
	if( ! mFbo )
		return {};

	return mFbo->getColorTexture();
}

void RenderToTexture::render()
{
	if( ! mFbo )
		return;

	const ivec2 size = mFbo->getSize();

	gl::ScopedFramebuffer	fboScope( mFbo );
	gl::ScopedViewport		viewportScope( size );
	gl::ScopedMatrices		matScope;

	bool originUpperLeft = mFormat.mLoadTopDown;
	gl::setMatricesWindow( size, originUpperLeft );

	gl::clear( ColorA::zero() );
	mSignalDraw->emit();
}

} // namespace mason
