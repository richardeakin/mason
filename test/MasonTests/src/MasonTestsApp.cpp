#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "ui/Suite.h"
#include "mason/Hud.h"
#include "mason/Common.h"
#include "mason/Config.h"
#include "mason/Assets.h"
#include "mason/Profiling.h"

#include "HudTest.h"
#include "MiscTest.h"
//#include "BlendingTest.h"

#define USE_SECONDARY_SCREEN 1
#define MSAA 4

using namespace ci;
using namespace ci::app;
using namespace std;

class MasonTestsApp : public App {
  public:
	void setup() override;
	void resize() override;
	void keyDown( app::KeyEvent event ) override;
	void update() override;
	void draw() override;

	void reload();

	ui::SuiteRef	mSuite;
	bool			mDrawHud = true;
	bool			mDrawProfiling = false;
};

void MasonTestsApp::setup()
{
	ma::initialize( ma::getRepoRootPath() );

	mSuite = make_shared<ui::Suite>();

	mSuite->registerSuiteView<HudTest>( "hud" );
	mSuite->registerSuiteView<MiscTest>( "misc" );
	//mSuite->registerSuiteView<MotionTest>( "motion" );

	reload();
}

void MasonTestsApp::reload()
{
	ma::assets()->getFile( app::getAssetPath( "config.json" ), [this]( DataSourceRef dataSource ) {
		ma::config()->read( dataSource );

		size_t testIndex = (size_t)ma::config()->get<int>( "app", "test" );
		mSuite->select( testIndex );
	} );
}

void MasonTestsApp::keyDown( app::KeyEvent event )
{
	if( event.isControlDown() ) {
		if( tolower( event.getChar() ) == 'r' ) {
			CI_LOG_I( "reloading suite" );
			if( event.isShiftDown() ) {
				CI_LOG_I( "- clearing variables" );
				ma::hud()->clear();
			}
			mSuite->reload();
		}
		else if( event.getChar() == 'f' ) {
			CI_LOG_I( "toggling fullscreen" );
			setFullScreen( ! isFullScreen() );
		}
		else if( event.getChar() == 'h' ) {
			CI_LOG_I( "toggling Hud" );
			mDrawHud = ! mDrawHud;
			mSuite->setDrawUiEnabled( mDrawHud );
		}
		else if( event.getChar() == 'l' )
			mDrawProfiling = ! mDrawProfiling;
	}
}

void MasonTestsApp::resize()
{
	CI_LOG_I( "window size: " << getWindowSize() );
}

void MasonTestsApp::update()
{
	{
		CI_PROFILE_CPU( "Suite update" );
		mSuite->update();
	}

#if CI_PROFILING
	if( mDrawProfiling )
		perf::printProfiling();
#endif
}

void MasonTestsApp::draw()
{
	CI_PROFILE_CPU( "main draw" );

	gl::clear();

	{
		CI_PROFILE_CPU( "Suite draw" );
		mSuite->draw();
	}

	if( mDrawHud ) {
		CI_PROFILE_CPU( "Hud draw" );
		ma::hud()->draw();
	}

	CI_CHECK_GL();
}

void prepareSettings( App::Settings *settings )
{
	bool useSecondaryScreen = ( USE_SECONDARY_SCREEN && Display::getDisplays().size() > 1 );

	if( useSecondaryScreen ) {
		for( const auto &display : Display::getDisplays() ) {
			//CI_LOG_I( "display name: " << display->getName() );
			if( display->getName() == "Color LCD" ) {
				// macbook
				settings->setDisplay( display );
				settings->setWindowSize( 1280, 720 );
			}
			else if( display->getName() == "Generic PnP Monitor" ) {
				// gechic 1303i 13"touch display
				settings->setDisplay( display );
				settings->setFullScreen( true );
			}
		}
	}
	else {
#if defined( CINDER_MAC )
		vec2 windowPos = { 0, 0 };
#else
		vec2 windowPos = { 0, 24 };
#endif
		settings->setWindowPos( windowPos.x, windowPos.y );
		settings->setWindowSize( 960, 565 );
	}
}

CINDER_APP( MasonTestsApp, RendererGl( RendererGl::Options().msaa( MSAA ) ), prepareSettings )
