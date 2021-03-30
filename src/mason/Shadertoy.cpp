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

#include "mason/Shadertoy.h"

#include "mason/Assets.h"
#include "mason/Common.h"
#include "mason/extra/ImGuiStuff.h"

#include "cinder/gl/gl.h"
#include "cinder/app/App.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason {

namespace {


} // anonymous namespace

Shadertoy::Shadertoy()
{
	mConnections += app::getWindow()->getSignalMouseDown().connect( [this]( app::MouseEvent &event ) {
		if( event.isShiftDown() ) {
			mMouseDown = true;
			mMouseDownPos = event.getPos();
		}
	} );
	mConnections += app::getWindow()->getSignalMouseUp().connect( [this]( app::MouseEvent &event ) {
		mMouseDown = false;
	} );
}

void Shadertoy::load( const ma::Info &info, const ma::Info &sceneInfo )
{
	if( info.contains( "vertShader" ) ) {
		mDefaultVertPath = info.get<fs::path>( "vertShader" );
	}

	loadScene( sceneInfo );
}

void Shadertoy::save( ma::Info &info ) const
{
}

void Shadertoy::loadScene( const ma::Info &info )
{
	loadTextures( info );
	loadGlsl( info );
}

void Shadertoy::loadTextures( const ma::Info &info )
{
	if( info.contains( "textures" ) ) {
		int texUnit = 0;
		for( const auto &texInfo : info.get<vector<ma::Info>>( "textures" ) ) {
			if( texUnit > 3 ) {
				CI_LOG_E( "too many textures specified, skipping. " );
				break;
			}

			string uniformName = texInfo.get<string>( "uniformName" );;
			fs::path filePath = texInfo.get<string>( "filePath" );
			if( ! mDataPath.empty() ) {
				filePath = mDataPath / filePath;
			}

			auto format = gl::Texture::Format().wrap( GL_REPEAT );
			string filter = texInfo.get<string>( "filter", "nearest" );

			if( filter == "linear" ) {
				format.minFilter( GL_LINEAR ).magFilter( GL_LINEAR );
			}
			else if( filter == "mipmap" ) {
				format.mipmap( true ).minFilter( GL_LINEAR_MIPMAP_LINEAR );
			}

			bool vflip = texInfo.get<bool>( "vflip", false );
			format.loadTopDown( ! vflip );

			string type = texInfo.get<string>( "type", "2d" );
			gl::TextureBaseRef tex;
			if( type == "2d" ) {
				tex = gl::Texture::create( loadImage( filePath ), format );
			}
			//else if( type == "3d" ) {
			//	// TODO: support loading 3d textures
			//	//tex = gl::Texture::create( loadImage( fullFilePath ), format );
			//}
			else if( type == "cube" ) {
				auto cubeMapFormat = gl::TextureCubeMap::Format().mipmap().internalFormat( GL_RGB16F ).minFilter( GL_LINEAR_MIPMAP_LINEAR ).magFilter( GL_LINEAR ).wrap( GL_CLAMP_TO_EDGE );
				auto ext = filePath.extension();
				if( filePath.extension() == ".dds" ) {
					tex = gl::TextureCubeMap::createFromDds( loadFile( filePath ), cubeMapFormat );
				}
				else {
					tex = gl::TextureCubeMap::create( loadImage( filePath ), cubeMapFormat );
				}
			}
			else {
				CI_LOG_E( "unexpected texture type: " << type << ", uniform: " << uniformName );
				continue;
			}

			mActiveTextures.
				push_back( { tex, uniformName, (TextureUnit)texUnit } );
			texUnit += 1;
		}
	}

	// TODO: move this around for loop
	try {
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load random texture", exc );
	}
}

void Shadertoy::setTexture( int unit, const std::string &uniformName, const ci::gl::TextureBaseRef &tex )
{
	if( mActiveTextures.size() <= unit ) {
		mActiveTextures.resize( unit + 1 );
	}

	mActiveTextures[unit] = { tex, uniformName, (TextureUnit)unit };
}


void Shadertoy::loadGlsl( const ma::Info &sceneInfo )
{
	mVertPath = sceneInfo.get<fs::path>( "vertShader", mDefaultVertPath );
	mFragPath = sceneInfo.get<fs::path>( "fragShader" );

	mConnections += ma::assets()->getShader( mVertPath, mFragPath, [this]( gl::GlslProgRef glsl ) {

		glsl->uniform( "uResolution", getSize() );

		for( const auto &activeTexture : mActiveTextures ) {
			glsl->uniform( activeTexture.mUniformName, (int)activeTexture.mUnit );
		}

		for( const auto &active : mActiveTextures ) {
			// kludge to set uEnvMapMaxMip uniform only when radiance env map is asked for (expected by lighting.glsl)
			if( active.mUniformName == "uEnvMapRadiance" ) {
				int envMapNumMipMaps = floor( std::log2( active.mTex->getWidth() ) ) - 1;
				glsl->uniform( "uEnvMapMaxMip", (float) ( envMapNumMipMaps - 1 ) );
			}
		}

		for( const auto &mp : mUniformBlocks ) {
			glsl->uniformBlock( mp.second, (GLint)mp.first );
		}

		if( mBatchScene )
			mBatchScene->replaceGlslProg( glsl );
		else {
			mBatchScene = gl::Batch::create( geom::Rect( Rectf( 0, 0, 1, 1 ) ), glsl );
		}
	} );
}

void Shadertoy::setUniformBlock( const std::string &name, int binding )
{
	if( mUniformBlocks.find( binding ) != mUniformBlocks.end() ) {
		CI_LOG_W( "already have a binding at location: " << binding << " (previous name: " << mUniformBlocks[binding] << ", this name: " << name << ")" );
	}

	mUniformBlocks[binding] = name;

	if( mBatchScene ) {	
		const auto &glsl = mBatchScene->getGlslProg();
		for( const auto &mp : mUniformBlocks ) {
			glsl->uniformBlock( mp.second, (GLint)mp.first );
		}
	}
}

const ci::gl::GlslProgRef&	Shadertoy::getGlslProg() const
{
	if( mBatchScene )
		return mBatchScene->getGlslProg();

	// return a null GlslProg
	static gl::GlslProgRef sNullGlslProg;
	return sNullGlslProg;
}

void Shadertoy::setSize( const vec2 &size )
{
	mSize = size;

	if( mBatchScene ) {
		mBatchScene->getGlslProg()->uniform( "uResolution", vec2( mSize ) );
	}
}

void Shadertoy::update( double currentTime, double deltaTime, const CameraPersp &cam )
{
	if( ! mBatchScene )
		return;

	mBatchScene->getGlslProg()->uniform( "uTime", (float)currentTime );
	mBatchScene->getGlslProg()->uniform( "uDeltaTime", (float)deltaTime );

	mBatchScene->getGlslProg()->uniform( "uCamPos", cam.getEyePoint() );
	mBatchScene->getGlslProg()->uniform( "uCamDir", cam.getViewDirection() );
	mBatchScene->getGlslProg()->uniform( "uCamFocalLength", cam.getFocalLength() );
	mBatchScene->getGlslProg()->uniform( "uPrevCamPos", mPrevCamPos );
	mBatchScene->getGlslProg()->uniform( "uPrevCamDir", mPrevCamDir );

	//if( deltaTime > 0.0001 ) {
		mPrevCamPos = cam.getEyePoint();
		mPrevCamDir = cam.getViewDirection();
	//}
}

// TODO: does this draw need to happen from within the world clock for uniforms to be correct?
// - for per object velocity. Does it matter?
void Shadertoy::draw( const CameraPersp &cam )
{
	if( ! mBatchScene )
		return;

	{
		auto glsl = mBatchScene->getGlslProg();
		int loc = glsl->getUniformLocation( "iMouse" );
		if( loc != -1 && mMouseDown ) {
			glsl->uniform( loc, vec4( app::getWindow()->getMousePos(), mMouseDownPos ) );
		}
	}

	// draw scene
	{
		//gl::ScopedDepthTest scopedDepthTest( true, GL_ALWAYS );
		//gl::ScopedDepthWrite scopedDepthWrite( true );

		for( const auto &tex : mActiveTextures ) {
			if( tex.mTex ) {
				if( mBatchScene ) {
					mBatchScene->getGlslProg()->uniform( tex.mUniformName, (int)tex.mUnit );
				}
				gl::context()->pushTextureBinding( tex.mTex->getTarget(), tex.mTex->getId(), (uint8_t)tex.mUnit );
			}
		}

		gl::ScopedModelMatrix modelScope;
		gl::scale( mSize.x, mSize.y, 1.0f );
		mBatchScene->draw();

		for( const auto &tex : mActiveTextures ) {
			if( tex.mTex ) {
				gl::context()->popActiveTexture( (uint8_t)tex.mUnit );
			}
		}

	}
}

void Shadertoy::updateUI()
{
	im::Text( "default vert shader: %s", mDefaultVertPath.string().c_str() );

	im::Separator();
	im::Text( "scene" );

	im::Text( "vert shader: %s", mVertPath.string().c_str() );
	im::Text( "frag shader: %s", mFragPath.string().c_str() );

	// TODO: show textures
}

} // namespace mason
