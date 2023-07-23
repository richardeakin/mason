#pragma once

#include "mason/Mason.h"

#include "cinder/gl/Texture.h"
#include "cinder/gl/BufferObj.h"

// TODO: add gl namespace
namespace mason {

void pushTextureBinding( const ci::gl::TextureBaseRef &texture, uint8_t textureUnit );
void popTextureBinding( const ci::gl::TextureBaseRef &texture, uint8_t textureUnit );

//! Returns a string version of the gl::Texture format (removes "GL_" prefix)
const char* textureFormatToString( GLenum format );
//! Returns the number of bytes per pixel for this gl::Texture format
size_t textureBytesPerPixel( GLenum format );

// TODO: use ci::context()->bindBufferBase()
struct ScopedBindBufferBase : private ci::Noncopyable {
	ScopedBindBufferBase( const ci::gl::BufferObjRef &bufferObj, GLuint index )
		: mIndex( index ), mTarget( bufferObj->getTarget() )
	{
		glBindBufferBase( mTarget, mIndex, bufferObj->getId() );
	}

	ScopedBindBufferBase( GLenum target, GLuint index, GLuint id )
		: mTarget( target ), mIndex( index )
	{
		glBindBufferBase( mTarget, mIndex, id );
	}

	~ScopedBindBufferBase()
	{
		glBindBufferBase( mTarget, mIndex, 0 );
	}

private:
	GLenum		mTarget;
	GLuint		mIndex;
};

} // mason
