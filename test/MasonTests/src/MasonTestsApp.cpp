#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "ui/Suite.h"
#include "mason/Hud.h"
#include "mason/Common.h"
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

	ui::SuiteRef	mSuite;
	bool			mDrawHud = true;
	bool			mDrawProfiling = false;
};

void MasonTestsApp::setup()
{
	ma::initialize();

	mSuite = make_shared<ui::Suite>();

	//mSuite->registerSuiteView<MotionTest>( "motion" );
	mSuite->registerSuiteView<HudTest>( "hud" );
//	mSuite->registerSuiteView<BlendingTest>( "blending" );
	mSuite->registerSuiteView<MiscTest>( "misc" );

	mSuite->select( 0 );
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

CINDER_APP( MasonTestsApp, RendererGl( RendererGl::Options().msaa( MSAA ) ), []( App::Settings *settings ) {

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
		settings->setWindowPos( 0, 0 );
		settings->setWindowSize( 960, 565 );
	}
} )
