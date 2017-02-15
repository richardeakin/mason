/*
 Copyright (c) 2010, The Cinder Project, All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

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

#pragma once

#include "mason/Mason.h"

#include "cinder/Vector.h"
#include "cinder/Camera.h"
#include "cinder/app/MouseEvent.h"
#include "cinder/app/Window.h"

namespace mason {

//! Enables user interaction with a CameraPersp via the mouse
class MA_API CameraFpsUi {
 public:
	CameraFpsUi();
	//! Constructs a CameraFpsUi which manipulates \a camera directly (and consequently expects its pointer to remain valid). Optionally attaches to mouse/window signals of \a window, with priority \a signalPriority.
	CameraFpsUi( ci::CameraPersp *camera, const ci::app::WindowRef &window = nullptr, int signalPriority = 0 );
	CameraFpsUi( const CameraFpsUi &rhs );
	~CameraFpsUi();
	
	CameraFpsUi& operator=( const CameraFpsUi &rhs );

	//! Connects to mouseDown, mouseDrag, mouseWheel and resize signals of \a window, with optional priority \a signalPriority
	void connect( const ci::app::WindowRef &window, int signalPriority = 0 );
	//! Disconnects all signal handlers
	void disconnect();
	//! Returns whether the CameraFpsUi is connected to mouse and window signal handlers
	bool isConnected() const;
	//! Sets whether the CameraFpsUi will modify its CameraPersp either through its Window signals or through the various mouse*() member functions. Does not prevent resize handling.
	void enable( bool enable = true )	{ mEnabled = enable; }
	//! Prevents the CameraFpsUi from modifying its CameraPersp either through its Window signals or through the various mouse*() member functions. Does not prevent resize handling.
	void disable()						{ mEnabled = false; }
	//! Returns whether the CameraFpsUi will modify its CameraPersp either through its Window signals or through the various mouse*() member functions. Does not prevent resize handling.
	bool isEnabled() const				{ return mEnabled; }

	//! Signal emitted whenever the user modifies the camera
	ci::signals::Signal<void()>&	getSignalCameraChange();
	
	void mouseDown( ci::app::MouseEvent &event );
	void mouseUp( ci::app::MouseEvent &event );
	void mouseWheel( ci::app::MouseEvent &event );
	void mouseDrag( ci::app::MouseEvent &event );

	void keyDown( ci::app::KeyEvent &event );
	void keyUp( ci::app::KeyEvent &event );

	void mouseDown( const ci::vec2 &mousePos );
	void mouseUp( const ci::vec2 &mousePos );
	void mouseWheel( float increment );
	void mouseDrag( const ci::vec2 &mousePos, bool leftDown, bool middleDown, bool rightDown );

	//! This is called automatically if event signals are connected.
	void update();

	//! Returns a reference to the currently controlled CameraPersp
	const	ci::CameraPersp& getCamera() const		{ return *mCamera; }
	//! Specifices which CameraPersp should be modified
	void	setCamera( ci::CameraPersp *camera )	{ mCamera = camera; }

	//! Sets the size of the window in pixels when no WindowRef is supplied with connect()
	void	setWindowSize( const ci::ivec2 &windowSizePixels ) { mWindowSize = windowSizePixels; }

	//! Sets the amount of movement when a hotkey is pressed. Default is 1. Shift makes it 1/10th the movement
	void	setMoveIncrement( float incr )	{ mMoveIncrement = incr; }
	//! Sets the amount of movement when a hotkey is pressed. Default is 1. Shift makes it 1/10th the movement
	float	getMoveIncrement() const		{ return mMoveIncrement;}

 private:
	ci::ivec2	getWindowSize() const;
 
	ci::vec2				mInitialMousePos, mLookDelta;
	ci::CameraPersp			mInitialCam;
	ci::CameraPersp			*mCamera;
	bool					mLookEnabled = false;

	ci::vec3				mMoveDirection, mMoveAccel, mMoveVelocity;
	float					mMoveIncrement = 1.0f;

	
	ci::ivec2					mWindowSize; // used when mWindow is null
	ci::app::WindowRef			mWindow;
	bool						mEnabled;
	int							mSignalPriority;
	std::vector<ci::signals::Connection>	mEventConnections;
	ci::signals::Signal<void()>				mSignalCameraChange;
};

}; // namespace mason
