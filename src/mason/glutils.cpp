
#include "cinder/gl/Context.h"

#include "mason/glutils.h"

using namespace ci;
using namespace std;

namespace mason {

void pushTextureBinding( const gl::TextureBaseRef &texture, uint8_t textureUnit )
{
	gl::context()->pushTextureBinding( texture->getTarget(), texture->getId(), textureUnit );
}

void popTextureBinding( const gl::TextureBaseRef &texture, uint8_t textureUnit )
{
	gl::context()->popTextureBinding( texture->getTarget(), textureUnit );
}

const char* textureFormatToString( GLenum format )
{
	switch( format ) {
		case GL_RED: return "R8";
		case GL_R8: return "R8";
		case GL_R8UI: return "R8UI";
		case GL_RG: return "RG8";
		case GL_RG8: return "RG8";
		case GL_RGB: return "RGB8";
		case GL_RGB8: return "RGB8";
		case GL_RGBA: return "RGBA8";
		case GL_RGBA8: return "RGBA8";
		case GL_R32F: return "R32F";
		case GL_RG32F: return "RG32F";
		case GL_RGB32F: return "RGB32F";
		case GL_RGBA32F: return "RGBA32F";
		case GL_R16F: return "R16F";
		case GL_R16UI: return "R16UI";
		case GL_RG16F: return "RG16F";
		case GL_RGB16F: return "RGB16F";
		case GL_RGBA16F: return "RGBA16F";
		case GL_DEPTH_COMPONENT16: return "DEPTH16";
		case GL_DEPTH_COMPONENT24: return "DEPTH24";
		case GL_DEPTH_COMPONENT32: return "DEPTH32";
		default:;
	}

	return "(unknown format)";
}

} // mason
