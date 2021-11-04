#include "mason/scene/Suite.h"

#include "mason/Config.h"
#include "mason/imx/ImGuiStuff.h"

#include "cinder/app/App.h"
#include "cinder/Log.h"

using namespace std;
using namespace ci;
namespace im = ImGui;

namespace mason {

Suite::Suite()
{
	mConnections += mWorldClock.getSignalClockStep().connect( signals::slot( this, &Suite::updateScene ) );
	connectEvents();
}

void Suite::load( const std::string &key )
{
	mCurrentTest = mFactory.build( key );
	mCurrentTest->setLabel( key );

	ma::Info testInfo = ma::config()->get( key, ma::Info() );

	mCurrentTest->loadComponent( testInfo );
	mCurrentTest->layoutComponent( app::getWindowBounds() );

	// TODO: reset time, like what I do with Tracker
}

void Suite::layout()
{
	if( mCurrentTest ) {
		mCurrentTest->layoutComponent( app::getWindowBounds() );
	}
}

void Suite::update()
{
	mWorldClock.update( app::getElapsedSeconds() );
}

void Suite::updateScene()
{
	if( mCurrentTest ) {
		mCurrentTest->updateComponent( mWorldClock.getCurrentTime(), mWorldClock.getDeltaTime() );
	}
}

void Suite::draw()
{
	if( mCurrentTest ) {
		mCurrentTest->drawComponent();
	}
}

// ----------------------------------------------------------------------------------------------------
// UI
// ----------------------------------------------------------------------------------------------------

void Suite::updateUI()
{
	if( im::Begin( "Test Suite" ) ) {
		auto testLabels = mFactory.getAllKeys();

		if( im::CollapsingHeader( "time", nullptr, ImGuiTreeNodeFlags_DefaultOpen ) ) {

			bool paused = mWorldClock.isPaused();
			if( im::Checkbox( "pause (p)", &paused ) ) {
				mWorldClock.setPaused( paused );
				// TODO: maybe this should call a virtual Component::onPaused() so that triggering pause from the UI also pauses necessary update elements
			}

			bool fixedTimeStep = mWorldClock.isFixedTimeStepEnabled();
			if( im::Checkbox( "fixed time-step", &fixedTimeStep ) ) {
				mWorldClock.setFixedTimeStepEnabled( fixedTimeStep );
			}

			float targetFps = mWorldClock.getTargetFramesPerSecond();
			if( im::DragFloat( "target fps", &targetFps ) ) {
				mWorldClock.setTargetFramesPerSecond( targetFps );
			}


			float currentTime = (float)mWorldClock.getCurrentTime();
			if( paused ) {
				if( im::DragFloat( "time", &currentTime, 0.1f ) ) {
					mWorldClock.setCurrentTime( currentTime );
				}
			}
			else {
				im::Text( "time: %0.2f", currentTime );
			}
		}

		if( im::CollapsingHeader( ( "tests (" + to_string( testLabels.size() ) + ")###tests" ).c_str(), nullptr, ImGuiTreeNodeFlags_DefaultOpen ) ) {
			string currentLabel = mCurrentTest ? mCurrentTest->getLabel() : "none";

			int currentIndex = (int)( find( testLabels.begin(), testLabels.end(), currentLabel ) - testLabels.begin() );
			im::Text( "current index: %d, label: %s", currentIndex, currentLabel.c_str() );

			if( im::Combo( "tests", &currentIndex, testLabels ) ) {
				string key = testLabels[currentIndex];

				try {
					load( key );
				}
				catch( exception &exc ) {
					CI_LOG_EXCEPTION( "failed to load test with key: " << key, exc );
				}
			}
		}
	}

	im::End(); // "Test Suite"

	if( mCurrentTest ) {
		mCurrentTest->updateComponentUI();
	}
}

// ----------------------------------------------------------------------------------------------------
// Events
// ----------------------------------------------------------------------------------------------------

void Suite::connectEvents( const EventOptions &options )
{
	mEventSlotPriority = options.mPriority;
	mEventConnections.clear();

	if( ! mWindow ) {
		mWindow = app::getWindow();
	}

	if( options.mMouse && mWindow ) {
		mEventConnections += mWindow->getSignalMouseDown().connect( mEventSlotPriority, [&]( app::MouseEvent &event ) {
			app::TouchEvent touchEvent( event.getWindow(), vector<app::TouchEvent::Touch>( 1, app::TouchEvent::Touch( event.getPos(), vec2( 0 ), 0, 0, &event ) ) );
			touchesBegan( touchEvent );
			event.setHandled( touchEvent.isHandled() );
			mPrevMousePos = event.getPos();
		} );
		mEventConnections += mWindow->getSignalMouseDrag().connect( mEventSlotPriority, [&]( app::MouseEvent &event ) {
			app::TouchEvent touchEvent( event.getWindow(), vector<app::TouchEvent::Touch>( 1, app::TouchEvent::Touch( event.getPos(), mPrevMousePos, 0, 0, &event ) ) );
			touchesMoved( touchEvent );
			event.setHandled( touchEvent.isHandled() );
			mPrevMousePos = event.getPos();
		} );
		mEventConnections += mWindow->getSignalMouseUp().connect( mEventSlotPriority, [&]( app::MouseEvent &event ) {
			app::TouchEvent touchEvent( event.getWindow(), vector<app::TouchEvent::Touch>( 1, app::TouchEvent::Touch( event.getPos(), mPrevMousePos, 0, 0, &event ) ) );
			touchesEnded( touchEvent );
			event.setHandled( touchEvent.isHandled() );
			mPrevMousePos = event.getPos();
		} );
	}

	if( options.mTouches ) {
		mEventConnections += mWindow->getSignalTouchesBegan().connect( mEventSlotPriority, [&]( app::TouchEvent &event ) {
			touchesBegan( event );
		} );
		mEventConnections += mWindow->getSignalTouchesMoved().connect( mEventSlotPriority, [&]( app::TouchEvent &event ) {
			touchesMoved( event );
		} );
		mEventConnections += mWindow->getSignalTouchesEnded().connect( mEventSlotPriority, [&]( app::TouchEvent &event ) {
			touchesEnded( event );
		} );
	}

	if( options.mKeyboard ) {
		mEventConnections += mWindow->getSignalKeyDown().connect( mEventSlotPriority, [&]( app::KeyEvent &event ) {
			keyDown( event );
		} );
		mEventConnections += mWindow->getSignalKeyUp().connect( mEventSlotPriority, [&]( app::KeyEvent &event ) {
			keyUp( event );
		} );
	}
}

void Suite::disconnectEvents()
{
	mEventConnections.clear();
}


void Suite::keyDown( ci::app::KeyEvent &event )
{
	bool handled = false;
	if( mCurrentTest ) {
		handled = mCurrentTest->keyDown( event );
	}

	if( ! handled ) {
		if( event.getChar() == 'p' ) {
			mWorldClock.setPaused( ! mWorldClock.isPaused() );
			CI_LOG_I( "world clock paused: " << mWorldClock.isPaused() );
		}
	}
}

void Suite::keyUp( ci::app::KeyEvent &event )
{
	if( mCurrentTest )
		mCurrentTest->keyUp( event );
}

void Suite::touchesBegan( ci::app::TouchEvent &event )
{
	if( mCurrentTest ) {
		mCurrentTest->touchesBegan( event );
	}
}

void Suite::touchesMoved( ci::app::TouchEvent &event )
{
	if( mCurrentTest ) {
		mCurrentTest->touchesMoved( event );
	}
}

void Suite::touchesEnded( ci::app::TouchEvent &event )
{
	if( mCurrentTest ) {
		mCurrentTest->touchesEnded( event );
	}
}

} // namespace mason
