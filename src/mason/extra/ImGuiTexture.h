/*
Copyright (c) 2019, Richard Eakin - All rights reserved.

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

#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/CinderImGui.h"

namespace imx {

struct TextureViewerOptions {

	ImGuiTreeNodeFlags	mTreeNodeFlags = 0;
	bool				mOpenNewWindow = false; // TODO: also add option for whether it is in a collapseable header or not (might want it to be in a tree or somewhere else
	bool				mExtendedUI = false;
	float				mVolumeAtlasLineThickness = 2; //! setting to 0 disables
	ci::gl::GlslProgRef mGlsl;
	bool				mClearCachedOptions = false; //! Only used when getting the internal cached TextureViewer - allows updating options from outside C++ (will blow away internal options)

	enum class DebugPixelMode {
		Disabled,
		MouseClick,
		MouseHover
	};

	DebugPixelMode	mDebugPixelMode = DebugPixelMode::MouseHover;

	TextureViewerOptions&	openNewWindow( ImGuiTreeNodeFlags flags ) { mOpenNewWindow = flags; return *this; }
	TextureViewerOptions&	treeNodeFlags( ImGuiTreeNodeFlags flags ) { mTreeNodeFlags = flags; return *this; }
	TextureViewerOptions&	glsl( const ci::gl::GlslProgRef &glsl ) { mGlsl = glsl; return *this; }
	TextureViewerOptions&	extendedUI( bool enabled ) { mExtendedUI = enabled; return *this; }
	TextureViewerOptions&	openNewWindow( bool enabled ) { mOpenNewWindow = enabled; return *this; }
	TextureViewerOptions&	debugPixel( DebugPixelMode &mode ) { mDebugPixelMode = mode; return *this; }
	TextureViewerOptions&	clearCachedOptions( bool b = true )	{ mClearCachedOptions = b; return *this; }
};

void Texture2d( const char *label, const ci::gl::TextureBaseRef &texture, const TextureViewerOptions &options = TextureViewerOptions() );
void TextureDepth( const char *label, const ci::gl::TextureBaseRef &texture, const TextureViewerOptions &options = TextureViewerOptions() );
void TextureVelocity( const char *label, const ci::gl::TextureBaseRef &texture, const TextureViewerOptions &options = TextureViewerOptions() );
void Texture3d( const char *label, const ci::gl::TextureBaseRef &texture, const TextureViewerOptions &options = TextureViewerOptions() );

} // namespace imx
