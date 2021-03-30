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

#pragma once

#include "mason/Info.h"

#include "cinder/Camera.h"
#include "cinder/Signals.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/Texture.h"

namespace mason {

//! Loosely follows the shadertoy.com functionality
//! - draws a quad and proves configuration of textures / inputs
//! - a couple extras for things like motion blur and other post processing
class Shadertoy {
  public:
	Shadertoy();

	void load( const ma::Info &info, const ma::Info &sceneInfo );
	void save( ma::Info &info ) const;

	void loadScene( const ma::Info &info );

	void update( double currentTime, double deltaTime, const ci::CameraPersp &cam );
	void draw( const ci::CameraPersp &cam );
	void updateUI();

	void setSize( const ci::vec2 &size );
	const ci::vec2&	getSize() const	{ return mSize; }

	void setUniformBlock( const std::string &name, int binding );

	const ci::gl::GlslProgRef&	getGlslProg() const;

	const ci::fs::path& getDefaultVertPath() const	{ return mDefaultVertPath; }

	//! Where textures are searched for
	void setDataPath( const ci::fs::path &path ) { mDataPath = path; }

	void setTexture( int unit, const std::string &uniformName, const ci::gl::TextureBaseRef &tex );

  private:
	void loadTextures( const ma::Info &sceneInfo );
	void loadGlsl( const ma::Info &sceneInfo );

	ci::vec2						mSize;
	ci::vec3						mPrevCamPos, mPrevCamDir;
	ci::signals::ConnectionList		mConnections;
	ci::gl::BatchRef				mBatchScene;

	enum class TextureUnit {
		TEXTURE_0,
		TEXTURE_1,
		TEXTURE_2,
		TEXTURE_3,
	};

	struct ActiveTexture {
		ci::gl::TextureBaseRef	mTex;
		std::string				mUniformName;
		TextureUnit				mUnit;
	};

	std::vector<ActiveTexture>		mActiveTextures;
	std::map<int, std::string>		mUniformBlocks; // binding / name

	ci::fs::path mVertPath, mFragPath, mDataPath;

	bool mMouseDown = false;
	ci::vec2 mMouseDownPos;

	ci::fs::path	mDefaultVertPath;
};

} // namespace mason
