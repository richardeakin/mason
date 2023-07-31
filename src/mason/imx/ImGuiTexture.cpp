/*
 Copyright (c) 2019-23, Richard Eakin - All rights reserved.
 
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

#include "mason/imx/ImGuiTexture.h"
#include "mason/Assets.h"
#include "mason/glutils.h"

#include "cinder/gl/gl.h"
#include "cinder/Log.h"

using namespace std;
using namespace ci;
using namespace ImGui;

namespace imx {

// ----------------------------------------------------------------------------------------------------
// TextureViewer
// ----------------------------------------------------------------------------------------------------

class TextureViewer {
public:
	enum class Type {
		TextureColor,
		TextureVelocity,
		TextureDepth,
		Texture3d,
		Texture2dArray, // TODO: Add this type next to make sure that the random buffer is correct
		NumTypes
	};

	TextureViewer( const string &label, Type type )
		: mLabel( label ), mType( type )
	{
	}

	void view( const gl::TextureBaseRef &texture );

	void setOptions( const TextureViewerOptions &options )	{ mOptions = options; }
	void setTiledAtlasMode( bool enabled ) { mOptions.mTiledAtlasMode = enabled; }

	static TextureViewer*	getTextureViewer( const char *label, TextureViewer::Type type, const TextureViewerOptions &options );

private:
	void viewImpl( gl::FboRef &fbo, const gl::TextureBaseRef &texture );
	void updatePixelCoord( const gl::TextureBaseRef &texture );

	void renderColor( const gl::Texture2dRef &texture, const Rectf &destRect );
	void renderDepth( const gl::Texture2dRef &texture, const Rectf &destRect );
	void renderVelocity( const gl::Texture2dRef &texture, const Rectf &destRect );
	void render3d( const gl::Texture3dRef &texture, const Rectf &destRect );
	void render2dArray( const gl::Texture3dRef &texture, const Rectf &destRect );

	string		mLabel;
	Type		mType;
	gl::FboRef	mFbo, mFboNewWindow; // need a separate fbo for the 'new window' option to avoid imgui crash on stale texture id
	int			mNumTiles = -1; // TODO: tiles per row for Texture3D, total tiles (.z) for TextureArray
	int			mFocusedLayer = 0;

	vec4		mDebugPixel;
	ivec3		mDebugPixelCoord;
	bool        mDebugPixelNeedsUpdate = false;

	TextureViewerOptions mOptions;
};


const char *typeToString( TextureViewer::Type type )
{
	switch( type ) {
		case TextureViewer::Type::TextureColor: return "Color";
		case TextureViewer::Type::TextureVelocity: return "Velocity";
		case TextureViewer::Type::TextureDepth: return "Depth";
		case TextureViewer::Type::Texture3d: return "3d";
		case TextureViewer::Type::NumTypes: return "NumTypes";
		default: CI_ASSERT_NOT_REACHABLE();
	}

	return "(unknown)";
}

// static
TextureViewer* TextureViewer::getTextureViewer( const char *label, TextureViewer::Type type, const TextureViewerOptions &options )
{
	static map<ImGuiID, TextureViewer> sViewers;

	auto id = GetID( label );
	auto it = sViewers.find(  id );
	if( it == sViewers.end() ) {
		it = sViewers.insert( { id, TextureViewer( label, type ) } ).first;

		if( type == Type::Texture3d ) {
			// default to atlas although will let options override
			it->second.setTiledAtlasMode( true );
		}

		it->second.setOptions( options );
	}
	else if( options.mClearCachedOptions ) {
		it->second.setOptions( options );
	}

	// always update the glsl if it exists
	if( options.mGlsl ) {
		it->second.mOptions.mGlsl = options.mGlsl;
	}

	return &it->second;
}

void TextureViewer::view( const gl::TextureBaseRef &texture )
{
	ColorA headerColor = GetStyleColorVec4( ImGuiCol_Header );
	headerColor *= 0.65f;
	PushStyleColor( ImGuiCol_Header, headerColor );
	if( CollapsingHeader( mLabel.c_str(), mOptions.mTreeNodeFlags ) ) {
		viewImpl( mFbo, texture );
	}
	PopStyleColor();

	if( mOptions.mOpenNewWindow ) {
		SetNextWindowSize( vec2( 800, 600 ), ImGuiCond_FirstUseEver );
		if( Begin( mLabel.c_str(), &mOptions.mOpenNewWindow, ImGuiWindowFlags_AlwaysVerticalScrollbar ) ) {
			viewImpl( mFboNewWindow, texture );
		}
		End();
	}
}

void TextureViewer::viewImpl( gl::FboRef &fbo, const gl::TextureBaseRef &tex )
{
	if( ! tex ) {
		Text( "null texture" );
		return;
	}

	ScopedId idScope( mLabel.c_str() );

	// init or resize fbo if needed
	float availWidth = GetContentRegionAvail().x;
	if( ! fbo || abs( mFbo->getWidth() - availWidth ) > 4 ) {
		auto texFormat = gl::Texture2d::Format()
			.minFilter( GL_NEAREST ).magFilter( GL_NEAREST )
			.mipmap( false )
			.label( "TextureViewer (" + mLabel + ")" )
		;

		vec2 size = vec2( availWidth );
		if( mType != Type::Texture3d ) {
			float aspect = tex->getAspectRatio();
			size.y /= tex->getAspectRatio();
		}

		auto fboFormat = gl::Fbo::Format().colorTexture( texFormat ).samples( 0 ).label( texFormat.getLabel() );
		fbo = gl::Fbo::create( int( size.x ), int( size.y ), fboFormat );
	}

	if( mType == Type::Texture3d ) {
		Text( "size: [%d, %d, %d]", tex->getWidth(), tex->getHeight(), tex->getDepth() );
	}
	else {
		Text( "size: [%d, %d],", tex->getWidth(), tex->getHeight() );
	}
	SameLine();
	Text( "format: %s", ma::textureFormatToString( tex->getInternalFormat() ) );

	// show size of data in kilobytes
	size_t bytesPerPixel = ma::textureBytesPerPixel( tex->getInternalFormat() );
	size_t bytesTotal = tex->getWidth() * tex->getHeight() * tex->getDepth() * bytesPerPixel;
	string sizeType = "kb";
	float memoryUsed = float( bytesTotal ) / 1024.0f;
	if( memoryUsed > 1024.0f ) {
		sizeType = "mb";
		memoryUsed /= 1024.0f;
	}

	SameLine();
	Text( "memory: %0.2f %s, bytes per pixel: %d", memoryUsed, sizeType.c_str(), bytesPerPixel );

	// render to fbo based on current params
	{
		gl::ScopedFramebuffer fboScope( fbo );
		gl::ScopedViewport viewportScope( fbo->getSize() );
		gl::clear( ColorA::zero() );

		gl::ScopedDepth depthScope( false );
		gl::ScopedBlend blendScope( false );


		gl::ScopedMatrices matScope;
		gl::setMatricesWindow( fbo->getSize() );

		auto destRect = Rectf( vec2( 0 ), fbo->getSize() );
		if( mType == Type::TextureColor ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( tex );
			renderColor( texture2d, destRect );
		}
		else if( mType == Type::TextureVelocity ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( tex );
			renderVelocity( texture2d, destRect );
		}
		else if( mType == Type::TextureDepth ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( tex );
			renderDepth( texture2d, destRect );
		}
		else if( mType == Type::Texture3d ) {
			auto texture3d = dynamic_pointer_cast<gl::Texture3d>( tex );
			render3d( texture3d, destRect );
		}

		updatePixelCoord( tex );
	}

	if( mOptions.mExtendedUI ) {

		//Checkbox( "debug pixel", &options.mDebugPixelEnabled );
		//SameLine();
		//Checkbox( "use mouse", &mDebugPixelUseMouse );
		static vector<string> debugPixelModes = { "Disabled", "MouseClick", "MouseHover" };
		int                   t = (int)mOptions.mDebugPixelMode;
		SetNextItemWidth( 200 );
		if( Combo( "debug pixel", &t, debugPixelModes ) ) {
			mOptions.mDebugPixelMode = (DebugPixelMode)t;
		}

		SameLine();

		SetNextItemWidth( 300 );
		// TODO: fix this for non-square images
		if( DragInt3( "pixel coord", &mDebugPixelCoord, 0.5f, 0, tex->getWidth() - 1 ) ) {
			mDebugPixelNeedsUpdate = true;
		}
		SetNextItemWidth( 200 );
		DragFloat4( "pixel", &mDebugPixel );
	}

	// show texture that we've rendered to
	vec2 imagePos = ImGui::GetCursorScreenPos();
	Image( fbo->getColorTexture(), vec2( fbo->getSize() ) - vec2( 0.0f ) );

	bool pixelCoordNeedsUpdate = false;
	if( mOptions.mDebugPixelMode == DebugPixelMode::MouseClick && IsItemClicked() ) {
		mDebugPixelNeedsUpdate = true;
		pixelCoordNeedsUpdate = true;
	}
	else if( mOptions.mDebugPixelMode == DebugPixelMode::MouseHover && IsItemHovered() ) {
		mDebugPixelNeedsUpdate = true;
		pixelCoordNeedsUpdate = true;
	}

	if( pixelCoordNeedsUpdate ) {
		const float tiles = (float)mNumTiles;
		vec2 mouseNorm = ( vec2( GetMousePos() ) - vec2( GetItemRectMin() ) ) / vec2( GetItemRectSize() );
		vec3 pixelCoord;
		if( mOptions.mTiledAtlasMode ) {
			pixelCoord.x = fmodf( mouseNorm.x * (float)tex->getWidth() * tiles, (float)tex->getWidth() );
			pixelCoord.y = fmodf( mouseNorm.y * (float)tex->getHeight() * tiles, (float)tex->getHeight() );

			vec2 cellId = glm::floor( mouseNorm * tiles );
			pixelCoord.z = cellId.y * tiles + cellId.x;

		}
		else {
			pixelCoord.x = lround( mouseNorm.x * (float)tex->getWidth() );
			pixelCoord.y = lround( mouseNorm.y * (float)tex->getHeight() );
			pixelCoord.z = mFocusedLayer;
		}
		mDebugPixelCoord = glm::clamp( ivec3( pixelCoord ), ivec3( 0 ), ivec3( tex->getWidth(), tex->getHeight(), tex->getDepth() ) - ivec3( 1 ) );
	}


	if( mType == Type::Texture3d && mOptions.mTiledAtlasMode && mOptions.mVolumeAtlasLineThickness > 0.01f ) {
		// draw some grid lines over the image, using ImDrawList
		// - use current window background color for lines
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		auto col = GetStyleColorVec4( ImGuiCol_WindowBg );
		vec2 imageSize = GetItemRectSize();
		vec2 tileSize = imageSize / vec2( (float)mNumTiles );

		// note on drawing an extra line at x = 0 column and y = 0 row: it shouldn't be necessary,
		// but it is masking what appears to be an anti-aliasiang artifact in imgui drawlist
		for( int i = 0; i < mNumTiles; i++ ) {
			float thickness = i == 0 ? 1.0f : mOptions.mVolumeAtlasLineThickness;
			drawList->AddLine( imagePos + vec2( tileSize.x * (float)i, 0 ), imagePos + vec2( tileSize.x * (float)i, imageSize.y ), ImColor( col ), thickness );
			drawList->AddLine( imagePos + vec2( 0, tileSize.y * (float)i ), imagePos + vec2( imageSize.x, tileSize.y * (float)i ), ImColor( col ), thickness );
		}
	}

	OpenPopupOnItemClick( ( "##popup" + mLabel ).c_str() );
	if( BeginPopup( ( "##popup" + mLabel ).c_str() ) ) {
		Checkbox( "extended ui", &mOptions.mExtendedUI );
		if( Checkbox( "new window", &mOptions.mOpenNewWindow ) ) {
			if( ! mOptions.mOpenNewWindow ) {
				mFboNewWindow = nullptr;
			}
		}
		if( mType == Type::Texture3d ) {
			Checkbox( "atlas mode", &mOptions.mTiledAtlasMode );
			//DragInt( "tiles", &mNumTiles, 0.2f, 1, 1024 );
			DragFloat( "atlas line thikness", &mOptions.mVolumeAtlasLineThickness, 0.02f, 0, 4096 );
		}
		DragFloat( "scale", &mOptions.mScale, 0.01f, 0.02f, 1000.0f );
		DragFloat( "alpha override", &mOptions.mAlphaOverride, 0.004f, 0, 1 );
		if( mType == Type::TextureColor|| mType == Type::TextureDepth ) {
			Checkbox( "inverted", &mOptions.mInvertColor );
			Checkbox( "flip y", &mOptions.mFlipY );
		}

		EndPopup();
	}

	if( mOptions.mExtendedUI ) {
		if( mOptions.mGlsl ) {
			Text( "GlslProg: %s", ( ! mOptions.mGlsl->getLabel().empty() ? mOptions.mGlsl->getLabel().c_str() : "(unlabeled)" ) );
		}
	}
}

void TextureViewer::updatePixelCoord( const gl::TextureBaseRef &texture )
{
	if( ! mDebugPixelNeedsUpdate ) {
		return;
	}

	gl::ScopedTextureBind scopedTex0( texture, 0 );

	//glPixelStorei( GL_PACK_ALIGNMENT, 1 ); // TODO: needed?
	//glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	ivec3 pixelCoord = mDebugPixelCoord; // TODO: clamp so we can't crash

	vec4 pixel;
	const ivec3 pixelSize = { 1, 1, 1 };
	const GLint level = 0;
	GLenum format = GL_RGBA;
	GLenum dataType = GL_FLOAT;
	if( mType == TextureViewer::Type::TextureDepth ) {
		format = GL_DEPTH_COMPONENT;
	}

	// fetch one pixel from texture
	glGetTextureSubImage( texture->getId(), level,
		pixelCoord.x, pixelCoord.y, pixelCoord.z, pixelSize.x, pixelSize.y, pixelSize.z,
		format, dataType, sizeof( pixel ), &pixel.x );

	mDebugPixel = pixel;
	mDebugPixelNeedsUpdate = false;
}

void TextureViewer::renderColor( const gl::Texture2dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	// use static glsl if none provided
	auto glsl = mOptions.mGlsl;
	if( ! glsl ) {
		static gl::GlslProgRef sGlsl;
		if( ! sGlsl ) {
			const fs::path vertPath = "mason/textureViewer/texture.vert";
			const fs::path fragPath = "mason/textureViewer/textureColor.frag";
			ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
				sGlsl = glsl;
			} );
		}
		glsl = sGlsl;
	}

	// render to fbo based on current params
	if( glsl ) {
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		gl::ScopedGlslProg glslScope( glsl );
		glsl->uniform( "uScale", mOptions.mScale );
		glsl->uniform( "uInverted", mOptions.mInvertColor );
		glsl->uniform( "uFlipY", mOptions.mFlipY );

		gl::drawSolidRect( destRect );
	}
}

void TextureViewer::renderDepth( const gl::Texture2dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	// use static glsl if none provided
	auto glsl = mOptions.mGlsl;
	if( ! glsl ) {
		static gl::GlslProgRef sGlsl;
		if( ! sGlsl ) {
			const fs::path vertPath = "mason/textureViewer/texture.vert";
			const fs::path fragPath = "mason/textureViewer/textureDepth.frag";
			ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
				sGlsl = glsl;
			} );
		}
		glsl = sGlsl;
	}

	if( glsl) {
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		gl::ScopedGlslProg glslScope( glsl );
		glsl->uniform( "uScale", mOptions.mScale );
		glsl->uniform( "uInverted", mOptions.mInvertColor );
		glsl->uniform( "uFlipY", mOptions.mFlipY );

		gl::drawSolidRect( destRect );
	}
}


void TextureViewer::renderVelocity( const gl::Texture2dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	// use static glsl if none provided
	auto glsl = mOptions.mGlsl;
	if( ! glsl ) {
		static gl::GlslProgRef sGlsl;
		if( ! sGlsl ) {
			const fs::path vertPath = "mason/textureViewer/texture.vert";
			const fs::path fragPath = "mason/textureViewer/textureVelocity.frag";
			ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
				sGlsl = glsl;
			} );
		}
		glsl = sGlsl;
	}

	if( glsl ) {
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		gl::ScopedGlslProg glslScope( glsl );
		glsl->uniform( "uScale", mOptions.mScale );

		gl::drawSolidRect( destRect );
	}
}

void TextureViewer::render3d( const gl::Texture3dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	//if( mNumTiles < 0 ) {
	//	mNumTiles = (int)sqrt( texture->getDepth() ) + 1;
	//}

	mNumTiles = (int)sqrt( texture->getDepth() );

	// use static glsl if none provided
	auto glsl = mOptions.mGlsl;
	if( ! glsl ) {
		static gl::GlslProgRef sGlsl;
		if( ! sGlsl ) {
			const fs::path vertPath = "mason/textureViewer/texture.vert";
			const fs::path fragPath = "mason/textureViewer/texture3d.frag";
			ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
				sGlsl = glsl;
			} );
		}
		glsl = sGlsl;
	}

	// render to fbo based on current params
	{
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		if( glsl ) {
			gl::ScopedGlslProg glslScope( glsl );
			glsl->uniform( "uNumTiles", mNumTiles );
			glsl->uniform( "uFocusedLayer", mFocusedLayer );
			glsl->uniform( "uTiledAtlasMode", mOptions.mTiledAtlasMode );
			glsl->uniform( "uScale", mOptions.mScale );
			glsl->uniform( "uAlphaOverride", mOptions.mAlphaOverride );

			gl::drawSolidRect( destRect );
		}
	}

	if( mOptions.mExtendedUI ) {
		// TODO: make this a dropdown to select mode (may have more than two)
		Checkbox( "atlas mode", &mOptions.mTiledAtlasMode );
		if( mOptions.mTiledAtlasMode ) {
			SameLine();
			Text( ", tiles: %d", mNumTiles );
		}
		else {
			SliderInt( "slice", &mFocusedLayer, 0, texture->getDepth() );
		}
	}
}

void TextureViewer::render2dArray( const gl::Texture3dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	if( texture->getTarget() != GL_TEXTURE_2D_ARRAY ) {
		ImGui::Text( "wrong data type (expected GL_TEXTURE_2D_ARRAY)" );
		return;
	}

	mNumTiles = texture->getDepth();

	// use static glsl if none provided
	auto glsl = mOptions.mGlsl;
	if( ! glsl ) {
		static gl::GlslProgRef sGlsl;
		if( ! sGlsl ) {
			const fs::path vertPath = "mason/textureViewer/texture.vert";
			const fs::path fragPath = "mason/textureViewer/texture2dArray.frag";
			ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
				sGlsl = glsl;
			} );
		}
		glsl = sGlsl;
	}

	// render to fbo based on current params
	{
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		if( glsl ) {
			gl::ScopedGlslProg glslScope( glsl );
			glsl->uniform( "uNumTiles", mNumTiles );
			glsl->uniform( "uFocusedLayer", mFocusedLayer );
			glsl->uniform( "uTiledAtlasMode", mOptions.mTiledAtlasMode );
			glsl->uniform( "uScale", mOptions.mScale );

			gl::drawSolidRect( destRect );
		}
	}

	// TODO: combine as a single extended3DUI if they stay the same
	if( mOptions.mExtendedUI ) {
		// TODO: make this a dropdown to select mode (may have more than two)
		Checkbox( "atlas mode", &mOptions.mTiledAtlasMode );
		if( mOptions.mTiledAtlasMode ) {
			SameLine();
			Text( ", tiles: %d", mNumTiles );
		}
		else {
			SliderInt( "slice", &mFocusedLayer, 0, texture->getDepth() );
		}
	}

}

void Texture2d( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	TextureViewer::getTextureViewer( label, TextureViewer::Type::TextureColor, options )->view( texture );
}

void TextureDepth( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	TextureViewer::getTextureViewer( label, TextureViewer::Type::TextureDepth, options )->view( texture );
}

void TextureVelocity( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	TextureViewer::getTextureViewer( label, TextureViewer::Type::TextureVelocity, options )->view( texture );
}

void Texture3d( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	TextureViewer::getTextureViewer( label, TextureViewer::Type::Texture3d, options )->view( texture  );
}

void Texture2dArray( const char *label, const ci::gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	TextureViewer::getTextureViewer( label, TextureViewer::Type::Texture2dArray, options )->view( texture  );
}

} // namespace imx
