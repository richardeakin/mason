#pragma once

#include "mason/Mason.h"

#include "cinder/gl/Texture.h"

namespace mason {

void pushTextureBinding( const ci::gl::TextureBaseRef &texture, uint8_t textureUnit );
void popTextureBinding( const ci::gl::TextureBaseRef &texture, uint8_t textureUnit );

//! slightly shorter names than gl::constantToString()
const char* textureFormatToString( GLenum format );

} // mason
