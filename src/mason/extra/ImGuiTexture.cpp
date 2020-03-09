#include "mason/extra/ImGuiTexture.h"
#include "mason/Assets.h"
#include "mason/glutils.h"

#include "cinder/gl/gl.h"

using namespace std;
using namespace ci;
using namespace ImGui;

namespace imx {

// ----------------------------------------------------------------------------------------------------
// TextureViewer
// ----------------------------------------------------------------------------------------------------

namespace {

class TextureViewer {
public:
	enum class Type {
		TextureColor,
		TextureVelocity,
		TextureDepth,
		Texture3d,
		NumTypes
	};

	TextureViewer( const string &label, Type type )
		: mLabel( label ), mType( type )
	{
	}

	void view( const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags );

private:
	void viewImpl( gl::FboRef &fbo, const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags );

	void renderColor( const gl::Texture2dRef &texture, const Rectf &destRect );
	void renderDepth( const gl::Texture2dRef &texture, const Rectf &destRect );
	void renderVelocity( const gl::Texture2dRef &texture, const Rectf &destRect );
	void render3d( const gl::Texture3dRef &texture, const Rectf &destRect );

	string		mLabel;
	Type		mType;
	gl::FboRef	mFbo, mFboNewWindow; // need a separate fbo for the 'new window' option to avoid imgui crash on stale texture id
	int			mNumTiles = -1;
	int			mFocusedLayer = 0;
	bool		mTiledAtlasMode = true;
	bool		mShowExtendedUI = false;
	bool		mNewWindow = false;
	float       mScale = 1;
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


TextureViewer*	getTextureViewer( const char *label, TextureViewer::Type type )
{
	static map<ImGuiID, TextureViewer> sViewers;

	auto id = GetID( label );
	auto it = sViewers.find(  id );
	if( it == sViewers.end() ) {
		it = sViewers.insert( { id, TextureViewer( label, type ) } ).first;
	}

	return &it->second;
}

void TextureViewer::view( const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags )
{
	ColorA headerColor = GetStyleColorVec4( ImGuiCol_Header );
	headerColor *= 0.65f;
	PushStyleColor( ImGuiCol_Header, headerColor );
	if( CollapsingHeader( mLabel.c_str(), flags ) ) {
		viewImpl( mFbo, texture, flags );
	}
	PopStyleColor();

	if( mNewWindow ) {
		SetNextWindowSize( vec2( 800, 600 ), ImGuiCond_FirstUseEver );
		if( Begin( mLabel.c_str(), &mNewWindow ) ) {
			viewImpl( mFboNewWindow, texture, flags );
		}
		End();
	}
}

void TextureViewer::viewImpl( gl::FboRef &fbo, const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags )
{
	if( ! texture ) {
		Text( "null texture" );
		return;
	}

	// init or resize fbo if needed
	float availWidth = GetContentRegionAvailWidth();
	if( ! fbo || fbo->getColorTexture()->getInternalFormat() != texture->getInternalFormat() || abs( mFbo->getWidth() - availWidth ) > 4 ) {
		auto texFormat = gl::Texture2d::Format()//.internalFormat( texture->getInternalFormat() )
			.minFilter( GL_NEAREST ).magFilter( GL_NEAREST )
			.mipmap( false )
			.label( "TextureViewer (" + mLabel + ")" )
		;

		vec2 size = vec2( availWidth );
		if( mType != Type::Texture3d ) {
			float aspect = texture->getAspectRatio();
			size.y /= texture->getAspectRatio();
		}

		auto fboFormat = gl::Fbo::Format().colorTexture( texFormat ).samples( 0 ).label( texFormat.getLabel() );
		fbo = gl::Fbo::create( int( size.x ), int( size.y ), fboFormat );
	}

	if( mType == Type::Texture3d ) {
		Text( "size: [%d, %d, %d]", texture->getWidth(), texture->getHeight(), texture->getDepth() );
	}
	else {
		Text( "size: [%d, %d],", texture->getWidth(), texture->getHeight() );
	}
	SameLine();
	Text( "format: %s", mason::textureFormatToString( texture->getInternalFormat() ) );

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
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( texture );
			renderColor( texture2d, destRect );
		}
		else if( mType == Type::TextureVelocity ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( texture );
			renderVelocity( texture2d, destRect );
		}
		else if( mType == Type::TextureDepth ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( texture );
			renderDepth( texture2d, destRect );
		}
		else if( mType == Type::Texture3d ) {
			auto texture3d = dynamic_pointer_cast<gl::Texture3d>( texture );
			render3d( texture3d, destRect );
		}
	}

	// show texture that we've rendered to
	Image( fbo->getColorTexture(), vec2( fbo->getSize() ) - vec2( 0.0f ) );

	OpenPopupOnItemClick( ( "##popup" + mLabel ).c_str() );
	if( BeginPopup( ( "##popup" + mLabel ).c_str() ) ) {
		Checkbox( "extended ui", &mShowExtendedUI );
		if( Checkbox( "new window", &mNewWindow ) ) {
			if( ! mNewWindow ) {
				mFboNewWindow = nullptr;
			}
		}
		if( mType == Type::Texture3d ) {
			Checkbox( "tiled / atlas mode", &mTiledAtlasMode );
			DragInt( "tiles", &mNumTiles, 0.2f, 1, 1024 );
		}
		DragFloat( "scale", &mScale, 0.01f, 0.02f, 1000.0f );

		EndPopup();
	}
}

void TextureViewer::renderColor( const gl::Texture2dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	static gl::GlslProgRef sGlsl;
	if( ! sGlsl ) {
		const fs::path vertPath = "mason/textureViewer/texture.vert";
		const fs::path fragPath = "mason/textureViewer/textureColor.frag";
		ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
			sGlsl = glsl;
		} );
	}

	// render to fbo based on current params
	if( sGlsl ) {
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		gl::ScopedGlslProg glslScope( sGlsl );
		sGlsl->uniform( "uScale", mScale );

		gl::drawSolidRect( destRect );
	}
}

void TextureViewer::renderDepth( const gl::Texture2dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	static gl::GlslProgRef sGlsl;
	if( ! sGlsl ) {
		const fs::path vertPath = "mason/textureViewer/texture.vert";
		const fs::path fragPath = "mason/textureViewer/textureDepth.frag";
		ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
			sGlsl = glsl;
		} );
	}

	if( sGlsl) {
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		gl::ScopedGlslProg glslScope( sGlsl );
		sGlsl->uniform( "uScale", mScale );

		gl::drawSolidRect( destRect );
	}
}


void TextureViewer::renderVelocity( const gl::Texture2dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	static gl::GlslProgRef sGlsl;
	if( ! sGlsl ) {
		const fs::path vertPath = "mason/textureViewer/texture.vert";
		const fs::path fragPath = "mason/textureViewer/textureVelocity.frag";
		ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
			sGlsl = glsl;
		} );
	}

	if( sGlsl ) {
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		gl::ScopedGlslProg glslScope( sGlsl );
		sGlsl->uniform( "uScale", mScale );

		gl::drawSolidRect( destRect );
	}
}

void TextureViewer::render3d( const gl::Texture3dRef &texture, const Rectf &destRect )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	if( mNumTiles < 0 ) {
		mNumTiles = (int)sqrt( texture->getDepth() ) + 1;
	}

	static gl::GlslProgRef sGlsl;
	if( ! sGlsl ) {
		const fs::path vertPath = "mason/textureViewer/texture.vert";
		const fs::path fragPath = "mason/textureViewer/texture3d.frag";
		ma::assets()->getShader( vertPath, fragPath, []( gl::GlslProgRef glsl ) {
			sGlsl = glsl;
		} );
	}

	// render to fbo based on current params
	{
		gl::ScopedTextureBind scopedTex0( texture, 0 );

		if( sGlsl ) {
			gl::ScopedGlslProg glslScope( sGlsl );
			sGlsl->uniform( "uNumTiles", mNumTiles );
			sGlsl->uniform( "uFocusedLayer", mFocusedLayer );
			sGlsl->uniform( "uTiledAtlasMode", mTiledAtlasMode );
			sGlsl->uniform( "uRgbScale", mScale );

			gl::drawSolidRect( destRect );
		}
	}

	if( mShowExtendedUI ) {
		string mode = mTiledAtlasMode ? "tiled" : "slice";
		Text( "mode: %s", mode.c_str() );
		if( mTiledAtlasMode ) {
			SameLine();
			Text( ", tiles: %d", mNumTiles );
		}

		if( ! mTiledAtlasMode ) {
			DragInt( "slice", &mFocusedLayer, 0.3f, 0, texture->getDepth() );
		}
	}
}

} // anon

void Texture2d( const char *label, const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags )
{
	getTextureViewer( label, TextureViewer::Type::TextureColor )->view( texture, flags );
}

void TextureDepth( const char *label, const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags )
{
	getTextureViewer( label, TextureViewer::Type::TextureDepth )->view( texture, flags );
}

void TextureVelocity( const char *label, const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags )
{
	getTextureViewer( label, TextureViewer::Type::TextureVelocity )->view( texture, flags );
}

void Texture3d( const char *label, const gl::TextureBaseRef &texture, ImGuiTreeNodeFlags flags )
{
	getTextureViewer( label, TextureViewer::Type::Texture3d )->view( texture, flags );
}

} // namespace imx
