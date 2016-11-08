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

#include "mason/ParticleSystemGpu.h"
#include "mason/Common.h"

using namespace ci;
using namespace std;

namespace mason {

//ParticleSystemGpu::ParticleSystemGpu( size_t numParticles, const ci::geom::BufferLayout &bufferLayout )
//	: mNumParticles( numParticles )
//{
//	for( size_t i = 0; i < mBuffers.size(); i++ ) {
//		auto &buf = mBuffers[i];
//
//		buf.mVao = gl::Vao::create();
//		gl::ScopedVao vaoScope( buf.mVao );
//
//		buf.mVbo = gl::Vbo::create( GL_ARRAY_BUFFER, initialData.size() * sizeof( vec4 ), initialData.data(), GL_STATIC_DRAW );
//		buf.mVbo->bind();
//
//		GLsizei stride = GLsizei( sizeof( vec4 ) * numAttribs );
//
//		for( GLuint a = 0; a < numAttribs; a++ ) {
//			size_t offset = a * sizeof( vec4 );
//			gl::vertexAttribPointer( a, 4, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)offset );
//			gl::enableVertexAttribArray( a );
//		}
//
//		buf.mTransformFeedback = gl::TransformFeedbackObj::create();
//		buf.mTransformFeedback->bind();
//		gl::bindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf.mVbo );
//		buf.mTransformFeedback->unbind();
//	}
//}

ParticleSystemGpu::ParticleSystemGpu( size_t numParticles, size_t numAttribs, const std::vector<vec4> &initialData )
	: mNumParticles( numParticles )
{
	// Sanity check.
	CI_ASSERT( numParticles == initialData.size() / numAttribs );

	for( size_t i = 0; i < mBuffers.size(); i++ ) {
		auto &buf = mBuffers[i];

		buf.mVao = gl::Vao::create();
		gl::ScopedVao vaoScope( buf.mVao );

		buf.mVbo = gl::Vbo::create( GL_ARRAY_BUFFER, initialData.size() * sizeof( vec4 ), initialData.data(), GL_STATIC_DRAW );
		buf.mVbo->bind();

		GLsizei stride = GLsizei( sizeof( vec4 ) * numAttribs );

		for( GLuint a = 0; a < numAttribs; a++ ) {
			size_t offset = a * sizeof( vec4 );
			gl::vertexAttribPointer( a, 4, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)offset );
			gl::enableVertexAttribArray( a );
		}

		buf.mTransformFeedback = gl::TransformFeedbackObj::create();
		buf.mTransformFeedback->bind();
		gl::bindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf.mVbo );
		buf.mTransformFeedback->unbind();
	}
}

void ParticleSystemGpu::update()
{
	if( ! mGlslUpdate )
		return;

	gl::ScopedGlslProg	glslScope( mGlslUpdate );
	gl::ScopedVao		vaoScope( mBuffers[mBufferWriteIndex].mVao.get() );

	gl::ScopedState		stateScope( GL_RASTERIZER_DISCARD, true );

	gl::setDefaultShaderVars();

	mBuffers[mBufferReadIndex].mTransformFeedback->bind();

	gl::beginTransformFeedback( GL_POINTS );
	gl::drawArrays( GL_POINTS, 0, (GLsizei)mNumParticles );
	gl::endTransformFeedback();

	swap( mBufferReadIndex, mBufferWriteIndex );
}

void ParticleSystemGpu::draw()
{
	if( ! mGlslRender )
		return;

	gl::ScopedVao           vaoScope( mBuffers[mBufferReadIndex].mVao.get() );
	gl::ScopedState         pointSizeScope( GL_PROGRAM_POINT_SIZE, mProgramPointSizeEnabled );

	gl::ScopedGlslProg      glslScope( mGlslRender );

	gl::setDefaultShaderVars();
	gl::drawArrays( GL_POINTS, 0, (GLsizei)mNumParticles );
}

} // namespace mason
