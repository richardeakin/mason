/*
 Copyright (c) 2015, The Cinder Project
 
 This code is intended to be used with the Cinder C++ library, http://libcinder.org
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
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

#include "mason/FlyCam.h"
#include "cinder/app/AppBase.h"
#include "cinder/Log.h"

using namespace ci;
using namespace std;

namespace mason {

FlyCam::FlyCam()
	: mCamera( nullptr ), mWindowSize( 640, 480 ), mEnabled( true )
{
}

FlyCam::FlyCam( CameraPersp *camera, const app::WindowRef &window, const EventOptions &options )
	: mCamera( camera ), mInitialCam( *camera ), mWindowSize( 640, 480 ), mEnabled( true )
{
	connect( window, options );
}

FlyCam::FlyCam( const FlyCam &rhs )
	: mCamera( rhs.mCamera ), mInitialCam( *rhs.mCamera ), mWindowSize( rhs.mWindowSize ),
		mWindow( rhs.mWindow ), mEventOptions( rhs.mEventOptions ),	mEnabled( rhs.mEnabled ),
	mMoveIncrement( rhs.mMoveIncrement )
{
	connect( mWindow, mEventOptions );
}

FlyCam& FlyCam::operator=( const FlyCam &rhs )
{
	mCamera = rhs.mCamera;
	mInitialCam = *rhs.mCamera;
	mWindowSize = rhs.mWindowSize;
	mWindow = rhs.mWindow;
	mEventOptions = rhs.mEventOptions;
	mMoveIncrement = rhs.mMoveIncrement;
	mEnabled = rhs.mEnabled;
	connect( mWindow, mEventOptions );
	return *this;
}

FlyCam::~FlyCam()
{
	disconnect();
}

//! Connects to mouseDown, mouseDrag, mouseWheel and resize signals of \a window, with optional priority \a signalPriority
void FlyCam::connect( const app::WindowRef &window, const EventOptions &options )
{
	mEventConnections.clear();
	mWindow = window;
	mEventOptions = options;
	if( window ) {
		if( options.mMouse ) {
			mEventConnections += window->getSignalMouseDown().connect( options.mPriority,
				[this]( app::MouseEvent &event ) { mouseDown( event ); } );
			mEventConnections += window->getSignalMouseUp().connect( options.mPriority,
				[this]( app::MouseEvent &event ) { mouseUp( event ); } );
			mEventConnections += window->getSignalMouseDrag().connect( options.mPriority,
				[this]( app::MouseEvent &event ) { mouseDrag( event ); } );
			mEventConnections += window->getSignalMouseWheel().connect( options.mPriority,
				[this]( app::MouseEvent &event ) { mouseWheel( event ); } );
			mEventConnections += window->getSignalKeyDown().connect( options.mPriority,
				[this]( app::KeyEvent &event ) { keyDown( event ); } );
		}
		if( options.mKeyboard ) {
			mEventConnections += window->getSignalKeyUp().connect( options.mPriority,
				[this]( app::KeyEvent &event ) { keyUp( event ); } );
		}
		if( options.mResize ) {
			mEventConnections += window->getSignalResize().connect( options.mPriority,
				[this]() {
					setWindowSize( mWindow->getSize() );
					if( mCamera )
						mCamera->setAspectRatio( mWindow->getAspectRatio() );
				}
			);
		}		
		if( options.mUpdate && app::AppBase::get() ) {
			mEventConnections += app::AppBase::get()->getSignalUpdate().connect( options.mPriority,
				[this] { update(); } );
		}
	}
}

//! Disconnects all signal handlers
void FlyCam::disconnect()
{
	mEventConnections.clear();

	mWindow = nullptr;
}

bool FlyCam::isConnected() const
{
	return mWindow != nullptr;
}

signals::Signal<void()>& FlyCam::getSignalCameraChange()
{
	return mSignalCameraChange;
}

void FlyCam::mouseDown( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	mouseDown( event.getPos() );
	event.setHandled();
}

void FlyCam::mouseUp( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	mouseUp( event.getPos() );
	event.setHandled();
}

void FlyCam::mouseWheel( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	mouseWheel( event.getWheelIncrement() );
	event.setHandled();
}

void FlyCam::mouseDrag( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	bool isLeftDown = event.isLeftDown();
	bool isMiddleDown = event.isMiddleDown() || event.isAltDown();
	bool isRightDown = event.isRightDown() || event.isControlDown();

	if( isMiddleDown )
		isLeftDown = false;

	mouseDrag( event.getPos(), isLeftDown, isMiddleDown, isRightDown );
	event.setHandled();
}

void FlyCam::mouseDown( const vec2 &mousePos )
{
	if( ! mCamera || ! mEnabled )
		return;

	mLookEnabled = true;
	mInitialMousePos = mousePos;
	mInitialCam = *mCamera;
}

void FlyCam::mouseUp( const vec2 &mousePos )
{
	mLookEnabled = false;
	mLookDelta = vec2( 0 );
	mInitialCam = *mCamera;
}

void FlyCam::mouseDrag( const vec2 &mousePos, bool leftDown, bool middleDown, bool rightDown )
{
	if( ! mCamera || ! mEnabled )
		return;

	mLookDelta = ( mousePos - mInitialMousePos ) / vec2( getWindowSize() );
	mLookDelta *= 2.75f;

	mSignalCameraChange.emit();
}

void FlyCam::mouseWheel( float increment )
{
	if( ! mCamera || ! mEnabled )
		return;

	mMoveDirection.y = mMoveIncrement * increment * 0.1f;
}

// TODO: need a way to disable using up these keys
void FlyCam::keyDown( ci::app::KeyEvent &event )
{
	// skip if any modifier key is being pressed, except shift which is used to decrease speed.
	if( event.isAltDown() || event.isControlDown() || event.isMetaDown() )
		return;

	bool handled = true;
	float moveAmount = mMoveIncrement;
	if( event.isShiftDown() )
		moveAmount *= 0.1f;

	const char c = tolower( event.getChar() );

	if( c == 'a' || event.getCode() == app::KeyEvent::KEY_LEFT ) {
		mMoveDirection.x = - moveAmount;
	}
	else if( c == 'd' || event.getCode() == app::KeyEvent::KEY_RIGHT ) {
		mMoveDirection.x = moveAmount;
	}
	else if( c == 'w' ) {
		mMoveDirection.y = moveAmount;
	}
	else if( c == 's' ) {
		mMoveDirection.y = - moveAmount;
	}
	else if( c == 'e' || event.getCode() == app::KeyEvent::KEY_UP ) {
		mMoveDirection.z = moveAmount;
	}
	else if( c == 'c' || event.getCode() == app::KeyEvent::KEY_DOWN ) {
		mMoveDirection.z = - moveAmount;
	}
	else
		handled = false;

	event.setHandled( handled );
}

void FlyCam::keyUp( ci::app::KeyEvent &event )
{
	bool handled = true;
	const char c = tolower( event.getChar() );
	if( c == 'a' || c == 'd' || event.getCode() == app::KeyEvent::KEY_LEFT || event.getCode() == app::KeyEvent::KEY_RIGHT ) {
		mMoveDirection.x = 0;
		mMoveAccel.x = 0;
	}
	else if( c == 'w' || c == 's' ) {
		mMoveDirection.y = 0;
		mMoveAccel.y = 0;
	}
	else if( c == 'e' || c == 'c' || event.getCode() == app::KeyEvent::KEY_UP || event.getCode() == app::KeyEvent::KEY_DOWN ) {
		mMoveDirection.z = 0;
		mMoveAccel.z = 0;
	}
	else
		handled = false;

	event.setHandled( handled );
}

void FlyCam::update()
{
	if( ! mCamera )
		return;

	if( mLookEnabled ) {
		quat orientation = mInitialCam.getOrientation();
		orientation = normalize( orientation * angleAxis( mLookDelta.x, vec3( 0, -1, 0 ) ) );
		orientation = normalize( orientation * angleAxis( mLookDelta.y, vec3( -1, 0, 0 ) ) );

		mCamera->setOrientation( orientation );
		mCamera->setWorldUp( vec3( 0, 1, 0 ) );
	}

	mMoveAccel += mMoveDirection;

	const float maxVelocity = mMoveIncrement * 5;

	vec3 targetVelocity = glm::clamp( mMoveAccel * 0.3f, vec3( -maxVelocity ), vec3( maxVelocity ) );
	mMoveVelocity = lerp( mMoveVelocity, targetVelocity, 0.3f );

//	if( glm::length( mMoveVelocity ) > 0.01 ) {
//		CI_LOG_I( "mMoveDirection: " << mMoveDirection << ", mMoveAccel: " << mMoveAccel << ", mMoveVelocity: " << mMoveVelocity );
//	}

	vec3 forward, right, up;
	forward = mCamera->getViewDirection();
	mCamera->getBillboardVectors( &right, &up );

	vec3 eye = mCamera->getEyePoint();
	eye += right * mMoveVelocity.x;
	eye += forward * mMoveVelocity.y;
	eye += up * mMoveVelocity.z;

	mCamera->setEyePoint( eye );

	const float drag = 0.2f;
	mMoveAccel *= 1 - drag;
}

ivec2 FlyCam::getWindowSize() const
{
	if( mWindow )
		return mWindow->getSize();
	else
		return mWindowSize;
}

}; // namespace mason
