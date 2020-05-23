#include "DebugScene.h"

#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
//#include "cinder/CinderImGui.h"
#include "mason/extra/ImGuiStuff.h"

#include "mason/Assets.h"
#include "mason/Config.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace {

template<typename T>
T meterToCenti( T x )
{
	return x * 100.0f;
}

template<typename T>
T centiToMeter( T x )
{
	return x * 0.01f;
}

template<typename T>
T metersToMilli( T x )
{
	return x * 1000.0f;
}

template<typename T>
T milliToMeters( T x )
{
	return x * 0.001f;
}

set<ck4a::JointType>	mExtendedJoints = {
	ck4a::JointType::EyeLeft,
	ck4a::JointType::EyeRight,
	ck4a::JointType::EarLeft,
	ck4a::JointType::EarRight,
	ck4a::JointType::Nose,
	ck4a::JointType::ThumbLeft,
	ck4a::JointType::ThumbRight,
};

} // anon

// ----------------------------------------------------------------------------------------------------
// Public
// ----------------------------------------------------------------------------------------------------

void DebugScene::load( const ma::Info &info, bool full )
{
	if( full ) {
		mCamInitialized = false;
	}

	mDrawEnabled = info.get( "drawEnabled", true );
	vec3 worldMin = info["worldMin"];
	vec3 worldMax = info["worldMax"];
	mWorldBounds = AxisAlignedBox( worldMin, worldMax );

	if( info.contains( "boxes" ) ) {
		mBoxes.clear();
		auto boxes = info.get<vector<ma::Info>>( "boxes" );
		for( const auto &boxInfo : boxes ) {
			SceneBox box;
			vec3 boxCenter = boxInfo.get( "center" , vec3( 165, 23.5f, -98 ) );
			vec3 boxSize = boxInfo.get( "size" , vec3( 68, 54, 93 ) );
			box.mBox.set( boxCenter - boxSize / 2.0f, boxCenter + boxSize / 2.0f );

			mBoxes.push_back( box );
		}
	}

	loadSceneObjects();

	// Only call initCam on initial load or ui button
	if( ! mCamInitialized ) {
		initCam();
	}

	CI_LOG_I( "complete." );
}

void DebugScene::save( ma::Info &info ) const
{
	info["drawEnabled"] = mDrawEnabled;
	info["worldMin"] = mWorldBounds.getMin();
	info["worldMax"] = mWorldBounds.getMax();

	{
		std::vector<ma::Info> boxes;
		for( const auto &box : mBoxes ) {
			ma::Info boxInfo;
			boxInfo["center"] = box.mBox.getCenter();
			boxInfo["size"] = box.mBox.getSize();
			boxes.push_back( boxInfo );
		}
		info.set( "boxes", boxes );
	}

	{
		ma::Info cameraInfo;
		cameraInfo["fov"] = mCam.getFov();
		cameraInfo["eyePos"] = mCam.getEyePoint();
		cameraInfo["eyeTarget"] = mCam.getPivotPoint();
		cameraInfo["orientation"] = mCam.getOrientation();
		cameraInfo["flyspeed"] = mFlyCam.getMoveIncrement();
		info["camera"] = cameraInfo;
	}

	CI_LOG_I( "complete." );
}

void DebugScene::draw( const ck4a::CaptureManager *capture )
{
	if( mBatchSphere ) {
		mBatchSphere->getGlslProg()->uniform( "uJointConfidence", -1 ); // set to disabled
		mBatchSphere->getGlslProg()->uniform( "uJointVelocity", vec3( 0 ) );
	}

	if( mBatchBackground ) {
		auto glsl = mBatchBackground->getGlslProg();
		glsl->uniform( "uResolution", vec2( app::getWindowSize() ) );
		glsl->uniform( "uCamPos", mCam.getEyePoint() );
		glsl->uniform( "uCamDir", mCam.getViewDirection() );
		gl::ScopedBlend blendScope( false );

		gl::ScopedModelMatrix modelScope;
		gl::scale( (float)app::getWindowWidth(), (float)app::getWindowHeight(), 1.0f );
		mBatchBackground->draw();
	}

	gl::ScopedBlend blendScope( false );
	gl::ScopedDepth depthScope( true );
	gl::ScopedMatrices matScope;
	gl::setMatrices( mCam );

	gl::ScopedModelMatrix modelScopeGlobal;

	if( mDrawRoom ) {
		gl::ScopedColor colorScope;

		if( mBatchRoomWires ) {
			mBatchRoomWires->draw();
		}

		// draw coordinate axes
		if( true ) {
			gl::ScopedModelMatrix modelScope;
			//gl::translate( vec3( mWorldBounds.getMin().x + 30, 0.1f, mWorldBounds.getMin().z + 30 ) );
			gl::translate( vec3( mWorldBounds.getMin().x + 5, 0.1f, mWorldBounds.getMin().z + 5 ) );


			gl::scale( vec3( 50 ) );

			gl::ScopedGlslProg glslScope( gl::getStockShader( gl::ShaderDef().color().lambert() ) );
			gl::drawCoordinateFrame();
		}

		if( mBatchFloor ) {
			auto glsl = mBatchRoomWires->getGlslProg();
			glsl->uniform( "uBodyMaxDistance", capture->getMaxBodyDistance() );

			gl::ScopedBlend blendScope( true );
			gl::ScopedDepth depthScope( false );
			gl::ScopedColor colorScope( 0.5f, 0.5f, 0.5f, 0.5f );
			mBatchFloor->draw();
		}

		// Draw the marker, which should be mapped to the physical room too
		{
			gl::ScopedDepth depthScope( true );

			vec3 markerScene = vec3( mMarkerPos.x, 0, mMarkerPos.y );

			gl::ScopedColor colorScope( Color( 1, 1, 0.2f ) );
			gl::ScopedModelMatrix modelScope;
			gl::translate( markerScene );

			// draw marker radius
			{
				gl::ScopedBlend blendScope( true );
				gl::ScopedDepth depthScope( false );

				const float flatScale = 5;
				gl::ScopedModelMatrix modelScope2;
				gl::ScopedColor colorScope( ColorA( 1, 0, 0, 0.7f ) );
				gl::rotate( vec3( float( M_PI / 2.0 ), 0, 0 ) );
				gl::drawSolidCircle( vec2( 0 ), 10 ); // TODO: add a circle batch
			}

			// drawing a testing ball
			if( false ) {
				gl::ScopedModelMatrix modelScope;
				gl::ScopedColor colorScope( Color( 0.8f, 0.1f, 0 ) );

				gl::translate( vec3( 0, 50, 0 ) );
				gl::scale( vec3( 5 ) );
				mBatchSphere->draw();
			}
		}
	}


	if( mDrawDebugCaptureDevices ) {
		drawCaptureDevices( capture );
	}

	for( auto &box : mBoxes ) {
		box.mHitTest = false;
	}

	if( mDrawDeviceBodies ) {
		drawDeviceBodies( capture );
	}
	//if( mDrawMergedBodies ) {
	//	for( const auto &mp : mMergedBodies ) {
	//		drawBody( mp.second, Color( 1, 1, 1 ) );
	//	}

	//	//if( mGestures ) {
	//	//	mGestures->drawDebug();
	//	//}
	//}

	if( mDrawHitTestBox && mBatchCube ) {
		for( const auto &box : mBoxes ) {
			// TODO: multiply color to make it hit
			Color col = box.mHitTest ? Color( 1, 1, 0 ) : box.mColor;
			gl::ScopedColor color( col );
			gl::ScopedModelMatrix modelScope;

			gl::translate( box.mBox.getCenter() );
			gl::scale( box.mBox.getSize() );

			mBatchCube->getGlslProg()->uniform( "uJointVelocity", vec3( 0 ) );
			mBatchCube->draw();
		}
	}

}

void DebugScene::updateUI()
{
	if( ! mUIEnabled ) {
		return;
	}

	if( ! im::Begin( "Scene", &mUIEnabled ) ) {
		im::End();
		return;
	}


	if( im::TreeNodeEx( "draw", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		im::Checkbox( "scene", &mDrawEnabled );
		im::Checkbox( "room", &mDrawRoom );
		im::Checkbox( "bodies (per-device)", &mDrawDeviceBodies );
		im::Checkbox( "bodies (merged)", &mDrawMergedBodies );
		im::Checkbox( "extended joints", &mDrawExtendedJoints );
		im::Checkbox( "head lazer beam", &mDrawHeadLazerBeam );

		im::TreePop();
	}

	if( im::TreeNodeEx( "head look at testing", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		im::Checkbox( "boxes", &mDrawHitTestBox );

		if( im::Button( "add box" ) ) {
			mBoxes.emplace_back();
			// TODO: add random color, but then allow it to be adjusted in the gui
		}
		im::SameLine();
		if( im::Button( "remove box" ) && ! mBoxes.empty() ) {
			mBoxes.pop_back();
		}

		// TODO: add another tree node for this
		for( int i = 0; i < mBoxes.size(); i++ ) {
			auto &box = mBoxes[i];

			vec3 boxCenter = box.mBox.getCenter();
			vec3 boxSize = box.mBox.getSize();
			if( im::DragFloat3( ( "box center##" + to_string( i ) ).c_str(), &boxCenter.x, 0.5f ) ) {
				box.mBox.set( boxCenter - boxSize / 2.0f, boxCenter + boxSize / 2.0f );
			}
			if( im::DragFloat3( ( "box size##" + to_string( i ) ).c_str(), &boxSize.x, 0.5f ) ) {
				box.mBox.set( boxCenter - boxSize / 2.0f, boxCenter + boxSize / 2.0f );
			}

		}

		im::TreePop();
	}

	im::Separator();

	// TODO: redefine the room based on size and origin
	// - this will be easier in the long run
	// - however, won't work if origin is not in the center of the room
	vec3 worldMin = mWorldBounds.getMin();
	vec3 worldMax = mWorldBounds.getMax();
	if( im::DragFloat3( "world min (cm)", &worldMin.x, 0.01f ) ) {
		mWorldBounds.set( worldMin, worldMax );
		loadSceneObjects();
	}
	if( im::DragFloat3( "world max (cm)", &worldMax.x, 0.01f ) ) {
		mWorldBounds.set( worldMin, worldMax );
		loadSceneObjects();
	}

	if( im::CollapsingHeader( "Camera", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		if( im::Button( "reset" ) ) {
			initCam();
		}
		im::SameLine();
		if( im::Button( "initial" ) ) {
			mCam = mInitialCam;
		}
		im::SameLine();
		if( im::Button( "top down" ) ) {
			mCam.lookAt( vec3( 0, mWorldBounds.getMax().y * 2.5, 0.1f ), vec3( 0 ), vec3( 0, 0, -1 ) );
		}
		im::SameLine();
		if( im::Button( "left" ) ) {
			float y = mWorldBounds.getCenter().y;
			mCam.lookAt( vec3( mWorldBounds.getMin().x * 4, y, 0 ), vec3( mWorldBounds.getMax().x, y, 0 ), vec3( 0, 1, 0 ) );
		}
		im::SameLine();
		if( im::Button( "right" ) ) {
			float y = mWorldBounds.getCenter().y;
			mCam.lookAt( vec3( mWorldBounds.getMax().x * 4, y, 0 ), vec3( mWorldBounds.getMin().x, y, 0 ), vec3( 0, 1, 0 ) );
		}

		// TODO: config is specified as eyePos / eyeTarget for lookAt(), but I loose the target after
		// FlyCam usage. Instead, try to use setOrientation() and save that to config (but might not need this for ps app)
		imx::BeginDisabled( true );

		//im::DragFloat2( "size", &mSize.x );

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
		if( im::DragFloat( "fov", &fov, 0.01f, 0, 150 ) ) {
			mCam.setFov( fov );
		}

		float fovH = mCam.getFovHorizontal();
		if( im::DragFloat( "fov (H)", &fovH, 0.01f, 1, 180 ) ) {
			mCam.setFovHorizontal( fovH );
		}

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

		// TODO: add imguigizmo
		//im::PushStyleColor( ImGuiCol_FrameBg, vec4( 0 ) );
		//quat rot = glm::inverse( mCam.getOrientation() );
		//imguiGizmo::resizeSolidOf( 2 );
		//if( ImGui::gizmo3D( "##gizmo1", rot, IMGUIZMO_DEF_SIZE ) ) {
		//	mCam.setOrientation( glm::inverse( rot ) );
		//}
		//im::PopStyleColor();
	}

	im::End();
}

// ----------------------------------------------------------------------------------------------------
// Private
// ----------------------------------------------------------------------------------------------------

void DebugScene::initCam()
{
	ma::Info camInfo;
	try {
		auto sceneConfig = ma::config()->get<ma::Info>( "scene" );
		camInfo = sceneConfig["camera"];
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "Failed to load config for scene.camera", exc );
	}

	float fov = camInfo.get( "fov", 45.0f );
	vec3 eyePos = camInfo.get( "eyePos", vec3( 0, 10, 400 ) );
	vec3 eyeTarget = camInfo.get( "eyeTarget", vec3( 0, 0, -1 ) );
	float flySpeed = camInfo.get( "flySpeed", 10.0f );
	float aspect = app::getWindowAspectRatio();
	mCam.setPerspective( fov, aspect, 1.0f, 10000.0f );
	mCam.lookAt( eyePos, eyeTarget );

	if( camInfo.contains( "orientation" ) ) {
		quat q = camInfo.get<quat>( "orientation" );
		mCam.setOrientation( q );
	}

	//mCam.setEyePoint( eyePos );
	//mCam.setWorldUp( vec3( 0, 1, 0 ) );
	//mCam.setViewDirection( vec3( 0, 0, -1 ) );

	mFlyCam = ma::FlyCam( &mCam, app::getWindow(), ma::FlyCam::EventOptions().priority( -1 ) );
	mFlyCam.setMoveIncrement( flySpeed );

	mInitialCam = mCam;
	mCamInitialized = true;
}

void DebugScene::loadSceneObjects()
{
	mConnections.clear();
	mBatchBackground = {};
	mBatchRoomWires = {};
	mBatchFloor = {};
	mBatchSphere = {};
	mBatchCube = {};
	mBatchArrow = {};

	// background rect
	mConnections += ma::assets()->getShader( "glsl/passthrough.vert", "glsl/sceneBackground.frag", [this]( gl::GlslProgRef glsl ) {
		glsl->setLabel( "sceneBackground" );
		if( mBatchBackground )
			mBatchBackground->replaceGlslProg( glsl );
		else {
			mBatchBackground = gl::Batch::create( geom::Rect( Rectf( 0, 0, 1, 1 ) ), glsl );
		}
	} );

	// room
	mConnections += ma::assets()->getShader( "glsl/sceneColor.vert", "glsl/sceneRoom.frag", [this]( gl::GlslProgRef glsl ) {
		glsl->setLabel( "sceneColor" );
		if( mBatchRoomWires ) {
			mBatchRoomWires->replaceGlslProg( glsl );
		}
		else {
			// floor plane
			ivec2 subdiv = ivec2( centiToMeter( mWorldBounds.getSize().x ), centiToMeter( mWorldBounds.getSize().z ) );
			auto plane = geom::WirePlane().size( ivec2( mWorldBounds.getSize().x, mWorldBounds.getSize().z ) ).subdivisions( subdiv )
				>> geom::Translate( vec3( 0, 0, 0 ) );

			// room box
			auto box = geom::WireCube().size( mWorldBounds.getSize() )
				>> geom::Translate( mWorldBounds.getCenter() );

			auto room = plane & box;
			mBatchRoomWires = gl::Batch::create( room, glsl );
		}

		if( mBatchFloor ) {
			mBatchFloor->replaceGlslProg( glsl );
		}
		else {
			// floor plane
			auto plane = geom::Plane().size( ivec2( mWorldBounds.getSize().x, mWorldBounds.getSize().z ) );
			mBatchFloor = gl::Batch::create( plane, glsl );
		}
	} );

	// TODO: load arrow batch
	// - not sure yet how I'll do it because I don't want to distort the arrow tips when it's really long
	//     - for the moment could

	// sphere
	mConnections += ma::assets()->getShader( "glsl/sceneObject.vert", "glsl/sceneObject.frag", [this]( gl::GlslProgRef glsl ) {
		glsl->setLabel( "sceneObject" );
		if( mBatchSphere ) {
			mBatchSphere->replaceGlslProg( glsl );
		}
		else {
			auto sphere = geom::Sphere().subdivisions( 50 );
			mBatchSphere = gl::Batch::create( sphere, glsl );
		}

		if( mBatchCube ) {
			mBatchCube->replaceGlslProg( glsl );
		}
		else {
			auto cube = geom::Cube();
			mBatchCube = gl::Batch::create( cube, glsl );
		}

	} );

}

void DebugScene::drawCaptureDevices( const ck4a::CaptureManager *capture )
{
	if( ! mBatchSphere ) {
		return;
	}

	mBatchSphere->getGlslProg()->uniform( "uJointConfidence", -1 ); // set to disabled
	mBatchSphere->getGlslProg()->uniform( "uJointVelocity", vec3( 0 ) );

	gl::ScopedDepth depthScope( true );

	for( const auto &device : capture->getDevices() ) {
		if( ! device->isEnabled() )
			continue;

		gl::ScopedColor colorScope( device->getDebugColor() );

		gl::ScopedModelMatrix modelScope;

		gl::translate( device->getPos() );
		gl::rotate( device->getOrientation() );

		// Draw orientation with coordinate frame, origin with sphere
		{
			gl::ScopedModelMatrix modelScope2;
			gl::scale( vec3( 15 ) );

			gl::ScopedGlslProg glslScope( gl::getStockShader( gl::ShaderDef().color().lambert() ) );
			gl::drawCoordinateFrame();
		}

		{
			// Draw origin
			gl::ScopedModelMatrix modelScope2;
			gl::scale( vec3( 4 ) );
			mBatchSphere->draw();
		}
	}

}

void DebugScene::drawBody( const ck4a::Body &body, const Color &boneColor )
{
	vec3 bodyColor = ck4a::getDebugBodyColor( body.getId() );
	for( const auto &jp : body.getJoints() ) {
		const auto &joint = jp.second;
		if( ! joint.isActive() ) {
			continue;
		}

		//if( joint.mType != ck4a::JointType::Head ) {
		//	continue;
		//}

		if( ! mDrawExtendedJoints && mExtendedJoints.count( jp.first ) ) {
			continue;
		}

		auto parentJoint = body.getJoint( joint.getParentJointType() );
		if( parentJoint && parentJoint->isActive() ) {
			gl::ScopedColor colorScope( boneColor );
			gl::ScopedLineWidth lineWidth( 2 );
			gl::drawLine( parentJoint->mPos, joint.mPos );
		}

		vec3 jointColor = bodyColor;
		if( mBatchSphere ) {
			float scale = 2;
			if( joint.mType == ck4a::JointType::Head ) {
				scale *= 2;
			}

			//if( mResolveJoints.count( joint.mType ) ) {
			//	jointColor = vec3( 1, 0, 0 );
			//}

			mBatchSphere->getGlslProg()->uniform( "uJointConfidence", (int)joint.mConfidence );
			mBatchSphere->getGlslProg()->uniform( "uJointVelocity", joint.mVelocity );

			gl::ScopedColor colorScope( jointColor.x, jointColor.y, jointColor.z );
			gl::ScopedModelMatrix modelScope;
			gl::translate( joint.mPos );
			gl::scale( vec3( scale ) );
			mBatchSphere->draw();
		}

		if( mDrawJointOrientations ) {
			//if( joint.mConfidence == ck4a::JointConfidence::Medium ) {
			if( joint.mType == ck4a::JointType::Head ) {
				gl::ScopedModelMatrix modelScope;
				gl::translate( joint.mPos );
				gl::rotate( joint.mOrientation );

				gl::scale( vec3( 15 ) );
				gl::ScopedGlslProg glslScope( gl::getStockShader( gl::ShaderDef().color().lambert() ) );
				gl::drawCoordinateFrame();
			}
		}

		if( joint.mType == ck4a::JointType::Head ) {
			// head joint is looking down -z in the joint's view space
			vec3 headDir = normalize( vec3( joint.mOrientation * vec4( 0, 0, -1, 1 ) ) );

			// draw 'laser beam' from head pos in the direction the joint is facing
			if( mDrawHeadLazerBeam ) {
				gl::ScopedModelMatrix modelScope;
				gl::ScopedColor colorScope( 0.7f, 0, 0.3f );
				gl::ScopedLineWidth lineWidth( 1 );

				gl::drawLine( joint.mPos, joint.mPos + headDir * 500.0f );
			}

			// create Ray and hit-test with box
			if( mDrawHitTestBox ) {
				auto ray = Ray( joint.mPos, headDir );

				for( auto &box : mBoxes ) {
					if( box.mBox.intersects( ray ) ) {
						box.mHitTest = true;
					}
				}
			}
		}
	}

}

void DebugScene::drawDeviceBodies( const ck4a::CaptureManager *capture )
{
	gl::ScopedDepth depthScope( true );

	for( const auto &device : capture->getDevices() ) {
		Color deviceColor = device->getDebugColor();
		auto bodies = device->getBodies();
		for( const auto &body : bodies ) {
			drawBody( body, deviceColor );
		}
	}
}
