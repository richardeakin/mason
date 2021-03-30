#pragma once

#include "mason/Info.h"
#include "cinder/Signals.h"

#include "cinder/app/KeyEvent.h"
#include "cinder/app/TouchEvent.h"

namespace mason::scene {

using ComponentRef = std::shared_ptr<class Component>;

//! Type for something drawn in the scene.
//! - a WorldClock manages for the update and draw loops
//! - virtual load() / save() methods allow for serializing config params, virtual updateUI() provides controls (imgui)
//!
// TODO: consider renaming to Element (because Component is widely used in ECS nomenclature)
// TODO: consider making this a light subclass on top of View, that has methods for load / save and imgui update / add to component list
// - right now thinking to leave it separate and very minimal. Also, might end up being more 3d which would be a bit different
class Component {
public:
	Component();

	//! methods called from owner
	void loadComponent( const ma::Info &info );
	void saveComponent( ma::Info &info ) const;
	void layoutComponent( const ci::Rectf &bounds );
	void updateComponent( double currentTime, double deltaTime );
	void drawComponent();

	void enabledUI();
	void updateComponentUI();

	bool isDrawEnabled() const	{ return mDrawEnabled; }
	bool isUpdateEnabled() const	{ return mDrawEnabled; }

	virtual bool keyDown( ci::app::KeyEvent &event )	{ return false; }
	virtual bool keyUp( ci::app::KeyEvent &event )		{ return false; }

	virtual bool touchesBegan( ci::app::TouchEvent &event )		{ return false; }
	virtual bool touchesMoved( ci::app::TouchEvent &event )		{ return false; }
	virtual bool touchesEnded( ci::app::TouchEvent &event )		{ return false; }

	const std::string&	getLabel() const	{ return mLabel; }
	void setLabel( const std::string &label )	{ mLabel = label; }

	const ci::vec2&	getPos() const	{ return mPos; }
	const ci::vec2&	getSize() const	{ return mSize; }

	ci::Rectf getBounds() const	{ return ci::Rectf( mPos, mSize ); }

protected:

	std::string mLabel;
	bool mUpdateEnabled = true;
	bool mDrawEnabled = true;
	bool mUIEnabled = true;

	ci::signals::ConnectionList mConnections;

private:

	//! Methods overridden by subclasses
	virtual void load( const ma::Info &info )	{}
	virtual void save( ma::Info &info ) const	{}
	virtual void layout()	{}
	virtual void update( double currentTime, double deltaTime ) {}
	virtual void draw() {}
	virtual void updateUI() {}

	// TODO: consider having Component2d and Component3d subclasses that handle bounds differently.
	// - may end up deciding that all can be either 2d or 3d
	// - going to research other component systems before that
	ci::vec2 mPos, mSize;
};

} // namespace mason::scene
