#pragma once

#include "mason/scene/Component.h"
#include "mason/Flycam.h"

namespace mason::scene {

//! Operates a ma::FlyCam and ci::CameraPersp, with load / save and ui
class Camera {
public:
	Camera();

	void lookAt( const ci::vec3 &eyePoint, const ci::vec3 &target, const ci::vec3 &worldUp = ci::vec3( 0, 1, 0 ) );

	ci::vec3	getPos() const					{ return mCam.getEyePoint(); }
	ci::vec3	getViewDir() const				{ return mCam.getViewDirection(); }

	void setFov( float fov )						{ mCam.setFov( fov ); }

	void	setMoveIncrement( float incr )			{ mFlyCam.setMoveIncrement( incr ); }

	void resetCam();

	const ci::CameraPersp&	getCameraPersp() const	{ return mCam; }
	ci::CameraPersp&		getCameraPersp()		{ return mCam; }

	const ci::mat4&	getPreviousViewMatrix() const	{ return mPrevViewMatrix; }

	void load( const ma::Info &info );
	void save( ma::Info &info )	const;
	void update( double currentTime, double deltaTime );
	void updateUI();


private:
	ma::FlyCam			mFlyCam;
	ci::CameraPersp		mCam, mInitialCam;
	ci::vec2			mSize;
	ci::mat4			mPrevViewMatrix;
};

} // namespace mason::scene
