#include "BlendingTest.h"

#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"
#include "cinder/Rand.h"
#include "cinder/ImageIo.h"

#include "mason/Common.h"
#include "mason/Notifications.h"
#include "mason/DartBag.h"
#include "cidart/dart.h"
#include "cppformat/format.h"

using namespace ci;
using namespace std;

namespace {

enum AttribIndex : GLuint {
	POS = 0,
	LOOKUPS,
	MOVEMENT,
};

GLenum blendFuncEnumFromString( const std::string &funcName );
GLenum depthFuncEnumFromString( const std::string &funcName );

} // anoynmous namesace

BlendingTest::BlendingTest()
{
	// init cam
	mCam.setPerspective( 45.0f, app::getWindowAspectRatio(), 0.1f, 1000.0f );
	mCam.lookAt( vec3( 0, 7, 40 ), vec3( 0, 2, 0 ), vec3( 0, 1, 0 ) );
	mCamInitial = mCam;

	mCamUi = CameraUi( &mCam, app::getWindow(), -1 );

	ma::initializeDartVM();

	setupParams();
	setupScene();
	setupUI();
}

void BlendingTest::setupParams()
{
	fs::path paramsScript = "particleBlending/params.dart";

	ma::bag()->add( &mNumParticlesA, "numParticlesA", 10 ).updateFn( [this] { setupScene(); } );
	ma::bag()->add( &mWorldDims, "worldDims", vec3( 10, 10, 10 ) ).updateFn( [this] { setupScene(); } );
	ma::bag()->add( &mBackgroundColor, "backgroundColor", ColorA::black() )
		.updateFn( [this] {
			getBackground()->setColor( mBackgroundColor() );
		} );

	// FIXME: this needs to return a scopable object so we don't crash on reloads
	ma::bag()->addParser( "groups", [this]( Dart_Handle handle ) {
		mGroups.clear();

		if( ! Dart_IsList( handle ) ) {
			CI_LOG_E( "expected type list for key 'groups'" );
			return;
		}

		intptr_t length;
		CIDART_CHECK( Dart_ListLength( handle, &length ) );

		for( intptr_t i = 0; i < length; i++ ) {
			Dart_Handle listItem = Dart_ListGetAt( handle, i );

			CI_ASSERT( Dart_IsMap( listItem ) );

			mGroups.push_back( ParticleGroup() );
			auto &group = mGroups.back();

			group.pos = cidart::getMapValueForKey<vec3>( listItem, "pos" );
			group.color = cidart::getMapValueForKey<ColorA>( listItem, "color" );
			group.depthTest = cidart::getMapValueForKey<bool>( listItem, "depthTest" );
			group.depthWrite = cidart::getMapValueForKey<bool>( listItem, "depthWrite" );
			group.enableBlend = cidart::getMapValueForKey<bool>( listItem, "enableBlend" );
			group.blendMode = cidart::getMapValueForKey<string>( listItem, "blendMode" );

			// would actually need depthFuncSrc and depthFuncDest, or possible 4 if alpha is to be handled differently
//			{
//				Dart_Handle enumHandle = cidart::getMapValueForKey( listItem, "blendFunc" );
//				Dart_Handle enumStrHandle = Dart_Invoke( enumHandle, cidart::toDart( "toString" ), 0, nullptr );
//				CIDART_CHECK( enumStrHandle );
//				group.blendFunc = cidart::getValue<string>( enumStrHandle );
//			}
			{
				Dart_Handle enumHandle = cidart::getMapValueForKey( listItem, "depthFunc" );
				Dart_Handle enumStrHandle = Dart_Invoke( enumHandle, cidart::toDart( "toString" ), 0, nullptr );
				CIDART_CHECK( enumStrHandle );
				group.depthFunc = cidart::getValue<string>( enumStrHandle );
			}

			string typeStr = cidart::getMapValueForKey<string>( listItem, "type" );
			if( typeStr == "particles" ) {
				group.type = ParticleGroup::Type::PARTICLES;
			}
			else if( typeStr == "sphere" ) {
				group.type = ParticleGroup::Type::SPHERE;
			}
			else if( typeStr == "rect" ) {
				group.type = ParticleGroup::Type::RECT;
			}
		}
	} );


	try {
		mScriptWatch = ma::FileWatcher::load( paramsScript, [this]( const fs::path &path ) {
			try {
				ma::bag()->run( loadFile( path ) );
			}
			catch( ci::Exception &exc ) {
				MA_LOG_EXCEPTION( "error loading " << path, exc );
			}
		} );
	}
	catch( ci::Exception &exc ) {
		MA_LOG_EXCEPTION( "error loading " << paramsScript, exc );
	}
}

void BlendingTest::setupScene()
{
	vec3 worldDims = mWorldDims;

	CI_LOG_V( "worldDims: " << worldDims );

	size_t subdiv = 20;
	auto grid = geom::WirePlane().size( ivec2( worldDims.x, worldDims.z ) ).subdivisions( ivec2( subdiv ) );
	auto box = geom::WireCube().size( ivec3( worldDims ) );

	auto colorShader = gl::getStockShader( gl::ShaderDef().color() );
	mBatchGrid = gl::Batch::create( grid, colorShader );
	mBatchBox = gl::Batch::create( box, colorShader );

	mBatchSphere = gl::Batch::create( geom::Sphere().subdivisions( 50 ), gl::getStockShader( gl::ShaderDef().color().lambert() ) );

	setupTF();
	setupGlsl();
}

void BlendingTest::setupTF()
{
	const size_t numVec4Attribs = 3;
	vector<vec4> attribData( mNumParticlesA * numVec4Attribs );

	for( size_t i = 0; i < mNumParticlesA; i++ ) {
		// initialize life data so that ages vary
		// start the particle as already expired so that it gets new pos, velocity and timeBorn params in update shader
		float maxAge = randFloat( 1.0f, 3.0f );
		float age = randFloat( maxAge, 2 * maxAge );
		vec4 life;
		life.x = age;
		life.y = maxAge;
		life.z = (float)i; // id
		life.w = 0;

		attribData[i * numVec4Attribs + 0] = vec4( 0 );
		attribData[i * numVec4Attribs + 1] = life;
		attribData[i * numVec4Attribs + 2] = vec4( 0 ); // movement
	}

	mParticleSystem = make_shared<ma::ParticleSystemGpu>( mNumParticlesA, numVec4Attribs, attribData );
}

void BlendingTest::setupGlsl()
{
	// load update shaders
	{
		fs::path vertPath = "particleBlending/particleBlendingUpdate.vert";
		std::vector<std::string> feedbackVaryings = { "oPosition", "oLookups", "oMovement" };

		try {
			mWatchGlslUpdate = ma::FileWatcher::load( vertPath, [this, feedbackVaryings] ( const fs::path &scriptPath ) {
				auto format = gl::GlslProg::Format()
					.vertex( loadFile( scriptPath ) )
					.attribLocation( "iPosition", AttribIndex::POS )
					.attribLocation( "iLookups", AttribIndex::LOOKUPS )
//					.attribLocation( "iMovement", AttribIndex::MOVEMENT )
					.feedbackVaryings( feedbackVaryings )
					.feedbackFormat( GL_INTERLEAVED_ATTRIBS )
					.addPreprocessorSearchDirectory( ma::getGlslDirectory() )
				;

				try {
					auto glsl = gl::GlslProg::create( format );

					mParticleSystem->setGlslUpdate( glsl );
					ma::NotificationCenter::post( ma::NOTIFY_RESOURCE_RELOADED );
				}
				catch( std::exception &exc ) {
					MA_LOG_EXCEPTION( "failed to compile GlslProg at: " << format.getVertexPath(), exc );
				}
			} );
		}
		catch( std::exception &exc ) {
			MA_LOG_EXCEPTION( "failed to load glsl: " << vertPath, exc );
		}
	}

	// load render shaders
	{
		vector<fs::path> shaderPaths = {
			"particleBlending/particleBlending.vert",
			"particleBlending/particleBlending.frag"
		};

		try {

			mWatchGlslRender = ma::FileWatcher::load( shaderPaths, [this] ( const std::vector<fs::path> &fullPaths ) {
				auto format = gl::GlslProg::Format()
					.vertex( loadFile( fullPaths[0] ) )
					.fragment( loadFile( fullPaths[1] ) )
					.attribLocation( "iPosition", AttribIndex::POS )
					.attribLocation( "iLookups", AttribIndex::LOOKUPS )
//					.attribLocation( "iMovement", AttribIndex::MOVEMENT )
					.addPreprocessorSearchDirectory( ma::getGlslDirectory() )
				;

				try {
					auto glsl = gl::GlslProg::create( format );

					mParticleSystem->setGlslRender( glsl );
					ma::NotificationCenter::post( ma::NOTIFY_RESOURCE_RELOADED );
				}
				catch( std::exception &exc ) {
					MA_LOG_EXCEPTION( "failed to compile GlslProg at: (" << format.getVertexPath() << ", " << format.getFragmentPath() << ")" , exc );
				}
			} );
		}
		catch( std::exception &exc ) {
			MA_LOG_EXCEPTION( "failed to load glsl: " << shaderPaths[0], exc );
		}
	}

	// load depth buffer preview shader
	{
		vector<fs::path> shaderPaths = {
			"particleBlending/depthTexture.vert",
			"particleBlending/depthTexture.frag"
		};

		try {
			mWatchGlslDepthTexture = ma::FileWatcher::load( shaderPaths, [this] ( const std::vector<fs::path> &fullPaths ) {
				auto format = gl::GlslProg::Format()
					.vertex( loadFile( fullPaths[0] ) )
					.fragment( loadFile( fullPaths[1] ) )
					.addPreprocessorSearchDirectory( ma::getGlslDirectory() )
				;

				try {
					mGlslDepthTexture = gl::GlslProg::create( format );
					ma::NotificationCenter::post( ma::NOTIFY_RESOURCE_RELOADED );
				}
				catch( std::exception &exc ) {
					MA_LOG_EXCEPTION( "failed to compile GlslProg at: (" << format.getVertexPath() << ", " << format.getFragmentPath() << ")" , exc );
				}
			} );
		}
		catch( std::exception &exc ) {
			MA_LOG_EXCEPTION( "failed to load glsl: " << shaderPaths[0], exc );
		}
	}
}

void BlendingTest::setupUI()
{
	mInfoLabel = make_shared<ui::LabelGrid>();
	mInfoLabel->setTextColor( Color::white() );
	mInfoLabel->getBackground()->setColor( ColorA( 0, 0, 0, 0.2f ) );

	addSubview( { mInfoLabel } );
}

void BlendingTest::layout()
{
	const float padding = 4;
	const int numRows = 2;
	vec2 infoSize = { 260, 20 * numRows };
	mInfoLabel->setBounds( { getSize() - infoSize - padding, getSize() - padding } ); // anchor bottom right

	auto fboFormat = gl::Fbo::Format()
		.depthTexture()
	;
	mFbo = gl::Fbo::create( getWidth(), getHeight(), fboFormat );
}

bool BlendingTest::keyDown( app::KeyEvent &event )
{
	bool handled = true;

	if( event.getChar() == 'f' ) {
		mDrawFrame = ! mDrawFrame;
	}
	else if( event.getChar() == 'x' )
		resetCam();
	else if( event.getChar() == 'c' )
		centerCam();
	else if( event.getChar() == 'p' )
		mPauseSim = ! mPauseSim;
	else if( event.getChar() == 'S' ) {
		mDrawParticleSystem = ! mDrawParticleSystem;
	}
	else if( event.getChar() == 's' ) {
		mDrawSpheres = ! mDrawSpheres;
	}
	else if( event.getChar() == 'w' ) {
		mDrawWireframe = ! mDrawWireframe;
	}
	else if( event.getChar() == 'W' ) {
		if( mFbo && mFbo->getColorTexture() ) {
			fs::path imagePath = "particleTex.png";
			writeImage( imagePath, mFbo->getColorTexture()->createSource() );
			CI_LOG_V( "wrote Fbo color texture to " << imagePath );
		}
	}
	else if( event.getChar() == 'o' ) {
		mDrawParticlesToFbo = ! mDrawParticlesToFbo;
		CI_LOG_V( "draw particles to Fbo: " << boolalpha << mDrawParticlesToFbo << dec );
	}
	else
		handled = false;

	return handled;
}

bool BlendingTest::touchesBegan( app::TouchEvent &event )
{
	return false;
}

void BlendingTest::resetCam()
{
	mCam = mCamInitial;
}

void BlendingTest::centerCam()
{
	mCam.lookAt( vec3( 0, 1.0f, 0 ), vec3( 0, 5.0f, -40 ), vec3( 0, 1, 0 ) );
	mCam.setPivotDistance( 0 );
}

void BlendingTest::update()
{
	float deltaTime = mPauseSim ? 0 : 1.0f / app::getFrameRate();
	mSimulationTime += deltaTime;

	if( mParticleSystem && mParticleSystem->isReady() ) {

		auto glslUpdate = mParticleSystem->getGlslUpdate();
		glslUpdate->uniform( "uTime", (float)mSimulationTime );
		glslUpdate->uniform( "uDeltaTime", deltaTime );

		auto glslRender = mParticleSystem->getGlslRender();
		glslRender->uniform( "uTime", (float)mSimulationTime );

		if( ! mPauseSim )
			mParticleSystem->update();
	}

	updateUI();
}

void BlendingTest::updateUI()
{
	mInfoLabel->setRow( 0, { "fps:",  fmt::format( "{}", app::App::get()->getAverageFps() ) } );
	mInfoLabel->setRow( 1, { "num groups:",  to_string( mGroups.size() ) } );
}

void BlendingTest::draw( ui::Renderer *ren )
{
	gl::ScopedColor colorScope;

	// draw to the particle fbo
	// TODO: why is color different when rendering to FBO?
	// - I think it is because I'm drawing the content twice (there is no else statement below..)
	// - hm on second though, i don't think that's true, there is another if( ! drawFbo ) below
	if( mDrawParticlesToFbo ) {
		gl::ScopedFramebuffer fboScope( mFbo );
		gl::ScopedViewport viewportScope( 0, 0, mFbo->getWidth(), mFbo->getHeight() );

		gl::ScopedMatrices matScope;
		gl::setMatrices( mCam );

		renderParticles();
	}

	// draw 3d content
	{
		gl::ScopedMatrices matScope;
		gl::setMatrices( mCam );

		if( mDrawFrame && mBatchBox && mBatchGrid ) {
			gl::color( ColorA::gray( 0.5f, 0.3f ) );
			mBatchGrid->draw();
			gl::color( ColorA::gray( 0.8f, 0.4f ) );
			mBatchBox->draw();
		}

		// TODO: move to CoordinateAxisView
		{
			const ivec2 viewportSize( 180, 90 );
			gl::ScopedViewport viewportScope( 0, 0, viewportSize.x, viewportSize.y );
			gl::ScopedGlslProg glslScope( gl::getStockShader( gl::ShaderDef().lambert().color() ) );
			gl::ScopedDepth depthScope( true );
			gl::ScopedMatrices matricesScope;

			CameraPersp coordFrameCam;
			coordFrameCam.setPerspective( 45, viewportSize.x / viewportSize.y, 0.1f, 100.0f );
			coordFrameCam.lookAt( vec3( 0, 0, 2.75f ), vec3( 0 ) );

			gl::setMatrices( coordFrameCam );
			gl::translate( vec3( -1, 0, 0 ) );
			gl::multModelMatrix( mat4( inverse( mCam.getOrientation() ) ) );

			// axisLength = 1.0f, float headLength = 0.2f, float headRadius = 0.05f
			gl::drawCoordinateFrame( 1, 0.25f, 0.09f );
		}

		if( ! mDrawParticlesToFbo ) {
			renderParticles();
		}
	}

	if( mDrawParticlesToFbo ) {
		gl::ScopedColor colorScope( Color::white() );

		auto colorTex = mFbo->getColorTexture();
		gl::draw( colorTex );

		// draw depth texture top left
		auto depthTex = mFbo->getDepthTexture();
		if( depthTex && mGlslDepthTexture ) {
			auto destRect = Rectf( depthTex->getBounds() ).getCenteredFit( Rectf( 0, 0, 300, 300 ), true );
			// destRect.moveULTo( vec2( 4 ) ); TODO: use this after PR is in
			{
				vec2 size = destRect.getSize();
				destRect.set( 4, 4, size.x + 4, size.y + 4 );
			}
			{
				gl::ScopedGlslProg glslScope( mGlslDepthTexture );
				gl::ScopedTextureBind texScope( depthTex );

				gl::drawSolidRect( destRect );
			}

			gl::ScopedColor red( 1, 0.75f, 0 );
			gl::drawStrokedRect( destRect );
		}

	}
}

void BlendingTest::renderParticles()
{
	gl::clear( ColorA::zero() ); // TODO: move this so it is only called when doing the fbo route

	if( mParticleSystem && mDrawParticleSystem && mParticleSystem->getGlslRender() ) {

		for( const auto &group : mGroups ) {

			// TODO: expose all GLenum values to script
			GLenum sfactor = GL_SRC_ALPHA;
			GLenum dfactor = GL_ONE_MINUS_SRC_ALPHA;
			if( group.blendMode == "additive" ) {
				sfactor = GL_SRC_ALPHA;
				dfactor = GL_ONE;
			}

			gl::context()->pushBoolState( GL_BLEND, group.enableBlend );
			gl::context()->pushBlendFuncSeparate( sfactor, dfactor, sfactor, dfactor );

			GLenum depthFunc = depthFuncEnumFromString( group.depthFunc );
//			gl::ScopedDepth depthScope( group.depthTest, group.depthWrite, depthFunc );

			gl::ScopedDepthTest depthTestScope( group.depthTest, depthFunc );
			gl::ScopedDepthWrite depthWriteScope( group.depthWrite );

			gl::ScopedModelMatrix modelScope;
			gl::translate( group.pos );
//
////			glDisable( GL_DEPTH_TEST );
//
//			// TODO NEXT: move this to ParticleGroup / script
//			if( group.depthWrite ) {
//				glDepthFunc( GL_ALWAYS );
//			}
//			else {
//				glDepthFunc( GL_LESS );
//			}

			if( group.type == ParticleGroup::Type::PARTICLES ) {
				mParticleSystem->getGlslRender()->uniform( "uColor", group.color );
				mParticleSystem->draw();
			}
			else if( group.type == ParticleGroup::Type::SPHERE ) {
				mBatchSphere->draw();
			}
			else if( group.type == ParticleGroup::Type::RECT ) {
				gl::ScopedGlslProg glslScope( gl::getStockShader( gl::ShaderDef().color() ) );
				gl::drawSolidRect( Rectf( -0.5f, -0.5f, 0.5f, 0.5f ) );
			}
			else {
				CI_ASSERT_NOT_REACHABLE();
			}

			gl::context()->popBoolState( GL_BLEND );
			gl::context()->popBlendFuncSeparate();
		}
	}
}


// ----------------------------------------------------------------------------------------------------
// GLenum Helpers
// ----------------------------------------------------------------------------------------------------

namespace {

GLenum depthFuncEnumFromString( const std::string &funcName )
{
	static map<string, GLenum>	sEnumMap;
	if( sEnumMap.empty() ) {
		sEnumMap["DepthFunc.GL_NEVER"] = GL_NEVER;
		sEnumMap["DepthFunc.GL_LESS"] = GL_LESS;
		sEnumMap["DepthFunc.GL_EQUAL"] = GL_EQUAL;
		sEnumMap["DepthFunc.GL_LEQUAL"] = GL_LEQUAL;
		sEnumMap["DepthFunc.GL_GREATER"] = GL_GREATER;
		sEnumMap["DepthFunc.GL_NOTEQUAL"] = GL_NOTEQUAL;
		sEnumMap["DepthFunc.GL_GEQUAL"] = GL_GEQUAL;
		sEnumMap["DepthFunc.GL_ALWAYS"] = GL_ALWAYS;
	}

	auto it = sEnumMap.find( funcName );
	CI_ASSERT( it != sEnumMap.end() );
	return it->second;
}

GLenum blendFuncEnumFromString( const std::string &funcName )
{
	static map<string, GLenum>	sEnumMap;
	if( sEnumMap.empty() ) {
		sEnumMap["BlendFunc.GL_ZERO"] = GL_ZERO;
		sEnumMap["BlendFunc.GL_ONE"] = GL_ONE;
		sEnumMap["BlendFunc.GL_ONE_MINUS_SRC_COLOR"] = GL_ONE_MINUS_SRC_COLOR;
		sEnumMap["BlendFunc.GL_DST_COLOR"] = GL_DST_COLOR;
		sEnumMap["BlendFunc.GL_ONE_MINUS_DST_COLOR"] = GL_ONE_MINUS_DST_COLOR;
		sEnumMap["BlendFunc.GL_SRC_ALPHA"] = GL_SRC_ALPHA;
		sEnumMap["BlendFunc.GL_ONE_MINUS_SRC_ALPHA"] = GL_ONE_MINUS_SRC_ALPHA;
		sEnumMap["BlendFunc.GL_DST_ALPHA"] = GL_DST_ALPHA;
		sEnumMap["BlendFunc.GL_ONE_MINUS_DST_ALPHA"] = GL_ONE_MINUS_DST_ALPHA;
		sEnumMap["BlendFunc.GL_CONSTANT_COLOR"] = GL_CONSTANT_COLOR;
		sEnumMap["BlendFunc.GL_ONE_MINUS_CONSTANT_COLOR"] = GL_ONE_MINUS_CONSTANT_COLOR;
		sEnumMap["BlendFunc.GL_CONSTANT_ALPHA"] = GL_CONSTANT_ALPHA;
		sEnumMap["BlendFunc.GL_ONE_MINUS_CONSTANT_ALPHA"] = GL_ONE_MINUS_CONSTANT_ALPHA;
		sEnumMap["BlendFunc.GL_SRC_ALPHA_SATURATE"] = GL_SRC_ALPHA_SATURATE;
		sEnumMap["BlendFunc.GL_SRC1_COLOR"] = GL_SRC1_COLOR;
		sEnumMap["BlendFunc.GL_ONE_MINUS_SRC_COLOR"] = GL_ONE_MINUS_SRC_COLOR;
		sEnumMap["BlendFunc.GL_SRC1_ALPHA"] = GL_SRC1_ALPHA;
		sEnumMap["BlendFunc.GL_ONE_MINUS_SRC_ALPHA"] = GL_ONE_MINUS_SRC_ALPHA;
	}

	auto it = sEnumMap.find( funcName );
	CI_ASSERT( it != sEnumMap.end() );
	return it->second;
}

} // anoynmous namesace
