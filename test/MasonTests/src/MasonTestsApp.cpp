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

#define USE_SECONDARY_SCREEN 1
#define MSAA 4
#define LIVEPP_ENABLED 1

#if LIVEPP_ENABLED

#include "mason/LivePPManager.h"

#endif

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

	mSuite->getSignalSuiteViewWillChange().connect( [this] {
		CI_LOG_I( "selecting test: " << mSuite->getCurrentKey() );
	} );

	reload();
}

void MasonTestsApp::reload()
{
	ma::loadConfig();

	try {
		auto appConfig = ma::config()->get<ma::Dictionary>( "app" );

		size_t testIndex = appConfig["test"];
		mSuite->select( testIndex );
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load reload", exc );
	}
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
	}
}

void MasonTestsApp::resize()
{
	CI_LOG_I( "window size: " << getWindowSize() );
}

void MasonTestsApp::update()
{
	ma::hud()->showInfo( 1, { "watches: ", to_string( FileWatcher::instance().getNumWatches() ) } );
	ma::hud()->showInfo( 2, { "watched files: ", to_string( FileWatcher::instance().getNumWatchedFiles() ) } );

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
#if LIVEPP_ENABLED
	bool liveppEnabled = ma::initLivePP( "../../../../../../tools/LivePP", "MasonTests" );
	CI_LOG_I( "Live++ " << string( liveppEnabled ? "enabled" : "disabled" ) );
#endif

	bool useSecondaryScreen = ( USE_SECONDARY_SCREEN && Display::getDisplays().size() > 1 );

	if( useSecondaryScreen ) {
		const auto &display = Display::getDisplays()[1];
		settings->setDisplay( display );
		settings->setFullScreen( true );
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
