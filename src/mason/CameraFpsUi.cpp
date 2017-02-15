/*
 Copyright (c) 2015, The Cinder Project: http://libcinder.org All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Portions of this code (C) Paul Houx
 All rights reserved.

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

#include "mason/CameraFpsUi.h"
#include "cinder/app/AppBase.h"
#include "cinder/Log.h"

using namespace ci;
using namespace std;

namespace mason {

CameraFpsUi::CameraFpsUi()
	: mCamera( nullptr ), mWindowSize( 640, 480 ), mEnabled( true )
{
}

CameraFpsUi::CameraFpsUi( CameraPersp *camera, const app::WindowRef &window, int signalPriority )
	: mCamera( camera ), mInitialCam( *camera ), mWindowSize( 640, 480 ), mEnabled( true )
{
	connect( window, signalPriority );
}

CameraFpsUi::CameraFpsUi( const CameraFpsUi &rhs )
	: mCamera( rhs.mCamera ), mInitialCam( *rhs.mCamera ), mWindowSize( rhs.mWindowSize ),
		mWindow( rhs.mWindow ), mSignalPriority( rhs.mSignalPriority ),
		mEnabled( rhs.mEnabled )
{
	connect( mWindow, mSignalPriority );
}

CameraFpsUi::~CameraFpsUi()
{
	disconnect();
}

CameraFpsUi& CameraFpsUi::operator=( const CameraFpsUi &rhs )
{
	mCamera = rhs.mCamera;
	mInitialCam = *rhs.mCamera;
	mWindowSize = rhs.mWindowSize;
	mWindow = rhs.mWindow;
	mSignalPriority = rhs.mSignalPriority;
	mEnabled = rhs.mEnabled;
	connect( mWindow, mSignalPriority );
	return *this;
}

//! Connects to mouseDown, mouseDrag, mouseWheel and resize signals of \a window, with optional priority \a signalPriority
void CameraFpsUi::connect( const app::WindowRef &window, int signalPriority )
{
	if( ! mEventConnections.empty() ) {
		for( auto &conn : mEventConnections )
			conn.disconnect();
	}

	mWindow = window;
	mSignalPriority = signalPriority;
	if( window ) {
		mEventConnections.push_back( window->getSignalMouseDown().connect( signalPriority,
			[this]( app::MouseEvent &event ) { mouseDown( event ); } ) );
		mEventConnections.push_back( window->getSignalMouseUp().connect( signalPriority,
			[this]( app::MouseEvent &event ) { mouseUp( event ); } ) );
		mEventConnections.push_back( window->getSignalMouseDrag().connect( signalPriority,
			[this]( app::MouseEvent &event ) { mouseDrag( event ); } ) );
		mEventConnections.push_back( window->getSignalMouseWheel().connect( signalPriority,
			[this]( app::MouseEvent &event ) { mouseWheel( event ); } ) );
		mEventConnections.push_back( window->getSignalKeyDown().connect( signalPriority,
			[this]( app::KeyEvent &event ) { keyDown( event ); } ) );
		mEventConnections.push_back( window->getSignalKeyUp().connect( signalPriority,
			[this]( app::KeyEvent &event ) { keyUp( event ); } ) );
		mEventConnections.push_back( window->getSignalResize().connect( signalPriority,
			[this]() {
				setWindowSize( mWindow->getSize() );
				if( mCamera )
					mCamera->setAspectRatio( mWindow->getAspectRatio() );
			}
		) );

		mEventConnections.push_back( app::AppBase::get()->getSignalUpdate().connect( signalPriority,
			[this] { update(); } ) );
	}
	else
		disconnect();
}

//! Disconnects all signal handlers
void CameraFpsUi::disconnect()
{
	for( auto &conn : mEventConnections )
		conn.disconnect();

	mWindow.reset();
}

bool CameraFpsUi::isConnected() const
{
	return mWindow != nullptr;
}

signals::Signal<void()>& CameraFpsUi::getSignalCameraChange()
{
	return mSignalCameraChange;
}

void CameraFpsUi::mouseDown( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	mouseDown( event.getPos() );
	event.setHandled();
}

void CameraFpsUi::mouseUp( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	mouseUp( event.getPos() );
	event.setHandled();
}

void CameraFpsUi::mouseWheel( app::MouseEvent &event )
{
	if( ! mEnabled )
		return;

	mouseWheel( event.getWheelIncrement() );
	event.setHandled();
}

void CameraFpsUi::mouseDrag( app::MouseEvent &event )
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

void CameraFpsUi::mouseDown( const vec2 &mousePos )
{
	if( ! mCamera || ! mEnabled )
		return;

	mLookEnabled = true;
	mInitialMousePos = mousePos;
	mInitialCam = *mCamera;
}

void CameraFpsUi::mouseUp( const vec2 &mousePos )
{
	mLookEnabled = false;
	mLookDelta = vec2( 0 );
	mInitialCam = *mCamera;
}

void CameraFpsUi::mouseDrag( const vec2 &mousePos, bool leftDown, bool middleDown, bool rightDown )
{
	if( ! mCamera || ! mEnabled )
		return;

	mLookDelta = ( mousePos - mInitialMousePos ) / vec2( getWindowSize() );
	mLookDelta *= 2.75f;

	mSignalCameraChange.emit();
}

void CameraFpsUi::mouseWheel( float increment )
{
	if( ! mCamera || ! mEnabled )
		return;	
}

// TODO: need a way to disable using up these keys
// - also shouldn't handle if any modifier keys are down
void CameraFpsUi::keyDown( ci::app::KeyEvent &event )
{
	bool handled = true;
	float moveAmount = mMoveIncrement;
	if( event.isShiftDown() )
		moveAmount = 0.1f;

	const char c = tolower( event.getChar() );

	if( c == 'a' ) {
		mMoveDirection.x = - moveAmount;
	}
	else if( event.getCode() == 'd' ) {
		mMoveDirection.x = moveAmount;
	}
	else if( c == 'w' ) {
		mMoveDirection.y = moveAmount;
	}
	else if( c == 's' ) {
		mMoveDirection.y = - moveAmount;
	}
	else if( c == 'e' ) {
		mMoveDirection.z = moveAmount;
	}
	else if( c == 'c' ) {
		mMoveDirection.z = - moveAmount;
	}
	else
		handled = false;

	event.setHandled( handled );
}

void CameraFpsUi::keyUp( ci::app::KeyEvent &event )
{
	bool handled = true;
	const char c = tolower( event.getChar() );
	if( c == 'a' || c == 'd' ) {
		mMoveDirection.x = 0;
		mMoveAccel.x = 0;
	}
	else if( c == 'w' || c == 's' ) {
		mMoveDirection.y = 0;
		mMoveAccel.y = 0;
	}
	else if( c == 'e' || c == 'c' ) {
		mMoveDirection.z = 0;
		mMoveAccel.z = 0;
	}
	else
		handled = false;

	event.setHandled( handled );
}

void CameraFpsUi::update()
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

	const float maxVelocity = 5;

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
}

ivec2 CameraFpsUi::getWindowSize() const
{
	if( mWindow )
		return mWindow->getSize();
	else
		return mWindowSize;
}

}; // namespace mason
