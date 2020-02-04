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

#pragma once

#include "mason/Mason.h"

#include "cinder/gl/Fbo.h"
#include "cinder/Signals.h"

namespace mason {

//! Helper class to ease rendering to a ci::gl::Texture2d
class RenderToTexture {
public:
	struct Format {
		//! Specifies whether the Texture should store scanlines top-down in memory. Default is \c false. Also marks Texture as top-down when \c true.
		// TODO: I'd like to make it so you can pass in your own gl::Texture::Format, but some things aren't exposed on that which I need when drawing (like whether the format is loadTopDown enabeled)
		Format&		loadTopDown( bool loadTopDown = true ) { mLoadTopDown = loadTopDown; return *this; }
		//! Specifies the gl::Texture's internalFormat.
		Format&		colorFormat( GLint colorFormat )	{ mColorFormat = colorFormat; return *this; }
		//! Sets the number of samples used for Multisample Anti-Aliasing (MSAA). Valid values are powers of 2 (0, 2, 4, 8, 16). Defaults to \c 0.
		Format&		msaa( int samples ) { mMsaaSamples = samples; return *this; }
		//! Returns the number of samples used for Multisample Anti-Aliasing (MSAA).
		int			getMsaa() const { return mMsaaSamples; }

	private:
		bool	mLoadTopDown = false;
		GLint	mColorFormat = GL_RGBA;
		int		mMsaaSamples = 8;

		friend RenderToTexture;
	};

	RenderToTexture( const ci::ivec2 &size = { 0, 0 }, const Format &format = Format() );

	void		setSize( const ci::ivec2 &size );
	ci::ivec2	getSize() const;

	void		setClearEnabled( bool enable )	{ mClearEnabled = enable; }
	bool		isClearEnabled() const			{ return mClearEnabled; }

	ci::signals::Signal<void ()>&	getSignalDraw()	{ return *mSignalDraw; }
	ci::gl::Texture2dRef			getTexture() const;

	//! Renders to texture, calling the draw signal as appropriate
	void render();

private:

	ci::gl::FboRef		mFbo;
	Format				mFormat;
	bool				mClearEnabled = true;

	std::unique_ptr<ci::signals::Signal<void ()>>	mSignalDraw;
};

} // namespace mason
