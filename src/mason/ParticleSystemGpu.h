/*
 Copyright (c) 2015, Richard Eakin - All rights reserved.

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

#include "cinder/gl/gl.h"
#include "cinder/GeomIo.h"

#include "mason/Mason.h"

namespace mason {

typedef std::shared_ptr<class ParticleSystemGpu>	ParticleSystemGpuRef;

class MA_API ParticleSystemGpu : private ci::Noncopyable {
  public:
	//! Constructs a new ParticleSystemGpu with \a numAttribs attrib components, all of type vec4.
	ParticleSystemGpu( size_t numParticles, size_t numAttribs, const std::vector<ci::vec4> &initialData );
	//!
//	ParticleSystemGpu( size_t numParticles, const ci::geom::BufferLayout &bufferLayout );

	void update();
	void draw();

	size_t getNumParticles() const	{ return mNumParticles; }
	const ci::gl::GlslProgRef&	getGlslUpdate() const	{ return mGlslUpdate; }	
	const ci::gl::GlslProgRef&	getGlslRender() const	{ return mGlslRender; }

	void setGlslUpdate( const ci::gl::GlslProgRef &glsl )	{ mGlslUpdate = glsl; }
	void setGlslRender( const ci::gl::GlslProgRef &glsl )	{ mGlslRender = glsl; }

	bool	isReady() const		{ return (bool)( mGlslUpdate && mGlslRender ); }

	void setProgramPointSizeEnabled( bool enable )	{ mProgramPointSizeEnabled = enable; }
	bool isProgramPointSizeEnabled() const			{ return mProgramPointSizeEnabled; }

  private:

	struct Buffer {
		ci::gl::VaoRef					mVao;
		ci::gl::VboRef					mVbo;
		ci::gl::TransformFeedbackObjRef	mTransformFeedback;
	};

	size_t					mNumParticles;
	std::array<Buffer, 2>	mBuffers;
	mutable size_t			mBufferWriteIndex = 0;
	mutable size_t			mBufferReadIndex = 1;

	ci::gl::GlslProgRef		mGlslUpdate, mGlslRender;

	bool					mProgramPointSizeEnabled = true;
};

} // namespace mason
