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

	void view( const gl::TextureBaseRef &texture, const TextureViewerOptions &options );

private:
	void viewImpl( gl::FboRef &fbo, const gl::TextureBaseRef &texture, const TextureViewerOptions &options );

	void renderColor( const gl::Texture2dRef &texture, const Rectf &destRect, const TextureViewerOptions &options );
	void renderDepth( const gl::Texture2dRef &texture, const Rectf &destRect, const TextureViewerOptions &options );
	void renderVelocity( const gl::Texture2dRef &texture, const Rectf &destRect, const TextureViewerOptions &options );
	void render3d( const gl::Texture3dRef &texture, const Rectf &destRect, const TextureViewerOptions &options );

	string		mLabel;
	Type		mType;
	gl::FboRef	mFbo, mFboNewWindow; // need a separate fbo for the 'new window' option to avoid imgui crash on stale texture id
	int			mNumTiles = -1;
	int			mFocusedLayer = 0;
	bool		mTiledAtlasMode = true;
	bool		mShowExtendedUI = false;
	bool		mNewWindow = false;
	bool		mInverted = false;
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

void TextureViewer::view( const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	ColorA headerColor = GetStyleColorVec4( ImGuiCol_Header );
	headerColor *= 0.65f;
	PushStyleColor( ImGuiCol_Header, headerColor );
	if( CollapsingHeader( mLabel.c_str(), options.mTreeNodeFlags ) ) {
		viewImpl( mFbo, texture, options );
	}
	PopStyleColor();

	// TODO: remove mNewWindow, just use the passed in options
	// - should probably make all options non-const then, so I can add gui constrols for them in context menu
	if( mNewWindow ) {
		SetNextWindowSize( vec2( 800, 600 ), ImGuiCond_FirstUseEver );
		if( Begin( mLabel.c_str(), &mNewWindow ) ) {
			viewImpl( mFboNewWindow, texture, options );
		}
		End();
	}
}

void TextureViewer::viewImpl( gl::FboRef &fbo, const gl::TextureBaseRef &tex, const TextureViewerOptions &options )
{
	if( ! tex ) {
		Text( "null texture" );
		return;
	}

	// init or resize fbo if needed
	float availWidth = GetContentRegionAvailWidth();
	if( ! fbo || fbo->getColorTexture()->getInternalFormat() != tex->getInternalFormat() || abs( mFbo->getWidth() - availWidth ) > 4 ) {
		auto texFormat = gl::Texture2d::Format()//.internalFormat( texture->getInternalFormat() )
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
	Text( "format: %s", mason::textureFormatToString( tex->getInternalFormat() ) );

	// show size of data in kilobytes
	// - TODO: need convenience routine for calcing one pixel's size
	size_t bytes = tex->getWidth() * tex->getHeight() * tex->getDepth() * ( sizeof( float ) * 4 );

	SameLine();
	Text( "memory: %0.2f kb", float( bytes ) / 1024.0f );

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
			renderColor( texture2d, destRect, options );
		}
		else if( mType == Type::TextureVelocity ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( tex );
			renderVelocity( texture2d, destRect, options );
		}
		else if( mType == Type::TextureDepth ) {
			auto texture2d = dynamic_pointer_cast<gl::Texture2d>( tex );
			renderDepth( texture2d, destRect, options );
		}
		else if( mType == Type::Texture3d ) {
			auto texture3d = dynamic_pointer_cast<gl::Texture3d>( tex );
			render3d( texture3d, destRect, options );
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
		if( mType == Type::TextureDepth ) {
			Checkbox( "inverted", &mInverted );
		}

		EndPopup();
	}
}

void TextureViewer::renderColor( const gl::Texture2dRef &texture, const Rectf &destRect, const TextureViewerOptions &options )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	// use static glsl if none provided
	auto glsl = options.mGlsl;
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
		glsl->uniform( "uScale", mScale );

		gl::drawSolidRect( destRect );
	}
}

void TextureViewer::renderDepth( const gl::Texture2dRef &texture, const Rectf &destRect, const TextureViewerOptions &options )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	// use static glsl if none provided
	auto glsl = options.mGlsl;
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
		glsl->uniform( "uScale", mScale );
		glsl->uniform( "uInverted", mInverted );

		gl::drawSolidRect( destRect );
	}
}


void TextureViewer::renderVelocity( const gl::Texture2dRef &texture, const Rectf &destRect, const TextureViewerOptions &options )
{
	if( ! texture ) {
		ImGui::Text( "%s null", mLabel.c_str() );
		return;
	}

	// use static glsl if none provided
	auto glsl = options.mGlsl;
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
		glsl->uniform( "uScale", mScale );

		gl::drawSolidRect( destRect );
	}
}

void TextureViewer::render3d( const gl::Texture3dRef &texture, const Rectf &destRect, const TextureViewerOptions &options )
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
	auto glsl = options.mGlsl;
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
			glsl->uniform( "uTiledAtlasMode", mTiledAtlasMode );
			glsl->uniform( "uRgbScale", mScale );

			gl::drawSolidRect( destRect );
		}
	}

	if( mShowExtendedUI ) {
		// TODO: make this a widget
		// - either button, or dropdown with two options
		string mode = mTiledAtlasMode ? "tiled" : "slice";
		Text( "mode: %s", mode.c_str() );
		if( mTiledAtlasMode ) {
			SameLine();
			Text( ", tiles: %d", mNumTiles );
		}
		else {
			// FIXME: horizontal spacing is messed up here
			// - maybe from some parent widget. Can try moving it in main app to debug
			SliderInt( "##slice", &mFocusedLayer, 0, texture->getDepth() );
			SameLine();
			InputInt( "slice", &mFocusedLayer, 1, 0, texture->getDepth() );
		}
	}
}

} // anon

void Texture2d( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	getTextureViewer( label, TextureViewer::Type::TextureColor )->view( texture, options );
}

void TextureDepth( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	getTextureViewer( label, TextureViewer::Type::TextureDepth )->view( texture, options );
}

void TextureVelocity( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	getTextureViewer( label, TextureViewer::Type::TextureVelocity )->view( texture, options );
}

void Texture3d( const char *label, const gl::TextureBaseRef &texture, const TextureViewerOptions &options )
{
	getTextureViewer( label, TextureViewer::Type::Texture3d )->view( texture, options  );
}

} // namespace imx
