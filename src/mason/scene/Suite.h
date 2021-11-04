#pragma once

#include "mason/Factory.h"
#include "mason/WorldClock.h"
#include "mason/scene/Component.h"

#include "cinder/app/TouchEvent.h"

namespace mason {

class Suite {
public:
	Suite();

	//! Registers a subclass of scene::Component with an associated \a key that will be displayed in UI.
	template<typename Y>
	void add( const std::string &key );

	void load( const std::string &key );

	std::vector<std::string>	getAllKeys() const	{ return mFactory.getAllKeys(); }

	void layout();
	void update();
	void updateUI();
	void draw();

	struct EventOptions {
		EventOptions() {}

		EventOptions& mouse( bool enable = true )		{ mMouse = enable; return *this; }
		EventOptions& touches( bool enable = true )		{ mTouches = enable; return *this; }
		EventOptions& keyboard( bool enable = true )	{ mKeyboard = enable; return *this; }
		EventOptions& priority( int priority )			{ mPriority = priority; return *this; }

	private:
		bool	mMouse		= true;
		bool	mTouches	= true;
		bool	mKeyboard	= true;
		int		mPriority	= -1;

		friend class Suite;
	};

	//! Connects event propagation methods to the Window's event signals
	void connectEvents( const EventOptions &options = EventOptions() );
	//! Disconnects all event propagation methods.
	void disconnectEvents();

	void keyDown( ci::app::KeyEvent &event );
	void keyUp( ci::app::KeyEvent &event );
	void touchesBegan( ci::app::TouchEvent &event );
	void touchesMoved( ci::app::TouchEvent &event );
	void touchesEnded( ci::app::TouchEvent &event );

private:
	void updateScene();

	Factory<scene::Component>		mFactory;
	scene::ComponentRef				mCurrentTest;
	ma::WorldClock					mWorldClock;
	ci::signals::ConnectionList		mConnections;

	ci::app::WindowRef				mWindow;
	ci::signals::ConnectionList		mEventConnections;
	int								mEventSlotPriority = 1;
	ci::vec2						mPrevMousePos;
};


template<typename Y>
void Suite::add( const std::string &key )
{
	mFactory.registerBuilder<Y>( key );
}

} // namespace mason
