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
	void saveConfig();

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

	mSuite->getSignalSuiteViewWillChange().connect( [this] {
		CI_LOG_I( "selecting test: " << mSuite->getCurrentKey() );
	} );

	reload();
}

void MasonTestsApp::reload()
{
	// TODO: this should work without getAssetPath() too
	ma::assets()->getFile( app::getAssetPath( "config.json" ), [this]( DataSourceRef dataSource ) {
		CI_LOG_I( "config.json reloaded" );

		ma::config()->read( dataSource );

		size_t testIndex = (size_t)ma::config()->get<int>( "app", "test" );
		mSuite->select( testIndex );
	} );
}

void MasonTestsApp::saveConfig()
{
	// first disable config.json watch, so it doesn't trigger an app reload
	ma::FileWatcher::instance()->disable( ma::config()->getTargetFilePath() );

	ma::config()->set<int>( "app", "test", mSuite->getCurrentIndex() );

	ma::config()->write();

	ma::FileWatcher::instance()->enable( ma::config()->getTargetFilePath() );
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
		else if( event.getChar() == 'l' ) {
			mDrawProfiling = ! mDrawProfiling;
		}
		else if( event.getChar() == 's' ) {
			CI_LOG_I( "saving config.json" );
			saveConfig();
		}
	}
}

void MasonTestsApp::resize()
{
	CI_LOG_I( "window size: " << getWindowSize() );
}

void MasonTestsApp::update()
{
	ma::hud()->showInfo( 1, { "watches: ", to_string( ma::FileWatcher::instance()->getNumWatches() ) } );
	ma::hud()->showInfo( 2, { "watched files: ", to_string( ma::FileWatcher::instance()->getNumWatchedFiles() ) } );

	{
		CI_PROFILE( "Suite update" );
		mSuite->update();
	}

#if CI_PROFILING
	if( mDrawProfiling )
		perf::printProfiling();
#endif
}

void MasonTestsApp::draw()
{
	CI_PROFILE( "main draw" );

	gl::clear();

	{
		CI_PROFILE( "Suite draw" );
		mSuite->draw();
	}

	if( mDrawHud ) {
		CI_PROFILE( "Hud draw" );
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
		ivec2 windowPos = { 0, 0 };
#else
		ivec2 windowPos = { 0, 24 };
#endif
		settings->setWindowPos( windowPos.x, windowPos.y );
		settings->setWindowSize( 960, 565 );
	}
}

CINDER_APP( MasonTestsApp, RendererGl( RendererGl::Options().msaa( MSAA ) ), prepareSettings )
