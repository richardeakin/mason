#include "mason/scene/Camera.h"
#include "mason/imx/ImGuiStuff.h"
#include "imGuIZMOquat.h"

#include "cinder/Log.h"
#include "cinder/app/AppBase.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason::scene {

Camera::Camera()
{
	// TODO: make this window-independant
	// - handle same way as FlyCam without a window (if no window, provide a size to Camera)
	auto window = app::getWindow();
	if( window ) {
		mSize = window->getSize();

		mFlyCam = ma::FlyCam( &mCam, app::getWindow(), ma::FlyCam::EventOptions().priority( -1 ).update( false ).resize( true ) );
	}
}

void Camera::load( const ma::Info &info )
{
	if( mSize.x < 0.0001f || mSize.y < 0.0001f ) {
		CI_LOG_E( "invalid size (" << mSize << ")" );
		return;
	}

	float aspect = mSize.x / mSize.y;

	float fov = info.get<float>( "fov", 35 );
	vec2 clip = info.get<vec2>( "clip", vec2( 0.1f, 10000.0f ) );
	vec3 eyePos = info.get<vec3>( "eyePos", vec3( 3.05f, 7.286f, 21.5f ) );
	vec3 eyeTarget = info.get<vec3>( "eyeTarget", vec3( -0.189f, -0.218f, -0.958f ) );

	mCam.setPerspective( fov, aspect, clip[0], clip[1] );
	mCam.lookAt( eyePos, eyeTarget );

	float speed = info.get<float>( "flySpeed", 0.1f );
	mFlyCam.setMoveIncrement( speed );

	mInitialCam = mCam;
}

void Camera::save( ma::Info &info )	const
{
	// TODO: how to only save params that are overriding defaults?
	// - using something like std::optional for each value?
	// - where does this get saved when some configs are being merged (eg shadertoy section + shadertoy's scene)
	info["fov"] = mCam.getFov();
	info["clip"] = vec2( mCam.getNearClip(), mCam.getFarClip() );
	info["eyePos"] = mCam.getEyePoint();
	info["eyeTarget"] = mCam.getPivotPoint();

	info["flySpeed"] = mFlyCam.getMoveIncrement();
}

void Camera::resetCam()
{
}

void Camera::lookAt( const ci::vec3 &eyePoint, const ci::vec3 &target, const ci::vec3 &worldUp )
{
	mCam.lookAt( eyePoint, target, worldUp );
}

void Camera::update( double currentTime, double deltaTime )
{
	mFlyCam.update();
}

void Camera::updateUI()
{
	if( im::Button( "reset" ) ) {
		mCam = mInitialCam;
	}

	// TODO: config is specified as eyePos / eyeTarget for lookAt(), but I loose the target after
	// FlyCam usage. Instead, try to use setOrientation() and save that to config (but might not need this for ps app)
	imx::BeginDisabled( true );

	im::DragFloat2( "size", &mSize.x );

	vec3 viewDir = mCam.getViewDirection();
	im::DragFloat3( "view dir", &viewDir.x );
	imx::EndDisabled();

	vec3 eyePos = mCam.getEyePoint();
	if( im::DragFloat3( "eye pos", &eyePos.x ) ) {
		//CI_LOG_I( "eye: " << eye );
		mCam.setEyePoint( eyePos );
		//mCam.setViewDirection( vec3( 0, 0, -1 ) );
		//mCam.setWorldUp( vec3( 0, 1, 0 ) );
	}

	float fov = mCam.getFov();
	if( im::DragFloat( "fov", &fov, 0.02f, 0, 150 ) ) {
		mCam.setFov( fov );
	}

	float fovH = mCam.getFovHorizontal();
	if( im::DragFloat( "fov (H)", &fovH, 0.02f, 1, 180 ) ) {
		mCam.setFovHorizontal( fovH );
	}

	im::Text( "focal length: %0.3f", mCam.getFocalLength() );

	vec2 clip = { mCam.getNearClip(), mCam.getFarClip() };
	if( im::DragFloat2( "clip", &clip.x, 0.1f, 0.1f, 10e6 ) ) {
		mCam.setNearClip( clip.x );
		mCam.setFarClip( clip.y );
	}

	bool flyEnabled = mFlyCam.isEnabled();
	if( im::Checkbox( "fly enabled", &flyEnabled ) ) {
		mFlyCam.enable( flyEnabled );
	}

	float flySpeed = mFlyCam.getMoveIncrement();
	if( im::DragFloat( "fly speed", &flySpeed, 0.01f, 0.05f, 100.0f ) ) {
		mFlyCam.setMoveIncrement( flySpeed );
	}

	im::PushStyleColor( ImGuiCol_FrameBg, vec4( 0 ) );
	quat rot = glm::inverse( mCam.getOrientation() );
	imguiGizmo::resizeSolidOf( 2 );
	if( ImGui::gizmo3D( "##gizmo1", rot, IMGUIZMO_DEF_SIZE ) ) {
		mCam.setOrientation( glm::inverse( rot ) );
	}
	im::PopStyleColor();
}

} // namespace mason::scene
