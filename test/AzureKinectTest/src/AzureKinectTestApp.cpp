// Force dedicated graphics card on laptop
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/FileWatcher.h"
#include "cinder/Timer.h"

#include "mason/Assets.h"
#include "mason/Common.h"
#include "mason/Config.h"
#include "mason/prepareAppSettings.h"
#include "mason/Profiling.h"
#include "mason/FlyCam.h"
#include "mason/extra/ImGuiStuff.h"
#include "mason/extra/LivePPManager.h"

#include "DebugScene.h"
#include "ck4a/CaptureManager.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

// TODO: move to cinder/CinderImGui.h
namespace ImGui {

struct ScopedItemWidth : public ci::Noncopyable {
	ScopedItemWidth( float itemWidth );
	~ScopedItemWidth();
};

ScopedItemWidth::ScopedItemWidth( float itemWidth )
{
	ImGui::PushItemWidth( itemWidth );
}
ScopedItemWidth::~ScopedItemWidth()
{
	ImGui::PopItemWidth();
}

}
//! config.json: master config, with comments
//! local.json: overrides anything in config.json locally, with comments, .gitignored.
//! user.json: config settings saved by the General "save user settings" button or ctrl + s, .gitignored.
const auto CONFIG_FILES = vector<fs::path>( { "config.json", "local.json", "user.json" } );

#define MSAA 8
#define DEBUG_GL 1



class AzureKinectTestApp : public app::App {
  public:
	AzureKinectTestApp();
	void setup() override;
	void keyDown( app::KeyEvent event ) override;
	void keyUp( app::KeyEvent event ) override;
	void resize() override;
	void update() override;
	void draw() override;

	void	onMouseDown( app::MouseEvent &event );
	void	onMouseUp( app::MouseEvent &event );
	void	onMouseDrag( app::MouseEvent &event );	
	void	onMouseWheel( app::MouseEvent &event );
	void	mouseMove( app::MouseEvent event ) override;

	void reload( bool fullReload );
	void updateMouseHidden();

	void save( bool full );
	void save( ma::Info &info, bool full ) const;

	void initImGui();
	void updateImGui();

	std::unique_ptr<ck4a::CaptureManager> mCapture;
	//std::unique_ptr<ck4a::GestureManager> mGestures;
	std::unique_ptr<DebugScene> mScene;

	bool mImGuiEnabled = true;
	bool mLogsUIEnabled = false;
	bool mProfilingUIEnabled = false;

	bool mMouseHidden = false;

	void mergeBodies();

	bool					mMergeBodiesEnabled = false; // TODO: move merging to CaptureManager
	map<string, ck4a::Body>	mMergedBodies;

	set<ck4a::JointType>	mResolveJoints;

	float mJointDistanceConsideredSame = 15;

	bool mDrawExtendedJoints = false; // eyes, ears, toes, thumbs

	//vec4 mDevParams;
};

void loadLoggerSettings()
{
	auto appConfig = ma::config()->get<ma::Info>( "app" );
	auto devConfig = ma::config()->get<ma::Info>( "dev" );

	bool fileLogging = appConfig.get( "fileLogging", false );
	if( fileLogging ) {
		// TODO: get .exe name using GetModuleFileName()
		log::makeOrGetLogger<log::LoggerFileRotating>( "logs", "AzureKinectTest.%Y.%m.%d_%H.%M.%S.log" );
	}

	int breakOnLogLevel = devConfig["breakOnLogLevel"];
	log::makeOrGetLogger<log::LoggerBreakpoint>()->setTriggerLevel( ( log::Level )breakOnLogLevel );

	bool timestamps = appConfig["logTimestamps"];
	if( timestamps ) {
		log::makeOrGetLogger<log::LoggerConsole>()->setTimestampEnabled( true );
	}

	CI_LOG_I( "complete." );
}

void prepareSettings( app::App::Settings *settings )
{
	ma::prepareAppSettings( settings, CONFIG_FILES ); 
	loadLoggerSettings();
}

AzureKinectTestApp::AzureKinectTestApp()
{
	auto devConfig = ma::config()->get<ma::Info>( "dev" );
	bool liveppEnabled = devConfig.get( "livepp", false );
	if( liveppEnabled ) {
		liveppEnabled = ma::initLivePP( "../../../../../../tools/LivePP", "AzureKinectTest" );
	}

	CI_LOG_I( "Live++ " << string( liveppEnabled ? "enabled" : "disabled" ) );

	CI_LOG_I( "gpu vendor: " << gl::getVendorString() << ", version: " << gl::getVendorString() );

	// init everything in mason except paths right at startup
	ma::initialize( ma::getRepoRootPath() );

	initImGui();

	int signalPriority = -2;
	getWindow()->getSignalMouseDown().connect( signalPriority, signals::slot( this, &AzureKinectTestApp::onMouseDown ) );
	getWindow()->getSignalMouseUp().connect( signalPriority, signals::slot( this, &AzureKinectTestApp::onMouseUp ) );
	getWindow()->getSignalMouseDrag().connect( signalPriority, signals::slot( this, &AzureKinectTestApp::onMouseDrag ) );
	getWindow()->getSignalMouseWheel().connect( signalPriority, signals::slot( this, &AzureKinectTestApp::onMouseWheel ) );
}

void AzureKinectTestApp::setup()
{
	disableFrameRate();

	try {
		auto appConfig = ma::config()->get<ma::Info>( "app" );

		mMouseHidden = appConfig.get( "mouseHidden", false );
		updateMouseHidden();
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to setup app config", exc );
	}

	try {
		auto devConfig = ma::config()->get<ma::Info>( "dev" );
		mImGuiEnabled = devConfig["imgui"];
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to setup app dev config", exc );
	}

	CI_LOG_I( "complete." );

	reload( false );

	CI_LOG_I( "sio::client connected." );
}

void AzureKinectTestApp::reload( bool fullReload )
{
	CI_LOG_I( "reloading, full: " << fullReload );

	try {
		loadLoggerSettings();
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to reload (config)", exc );
	}

	try {
		auto captureInfo = ma::config()->get<ma::Info>( "capture" );
		mCapture = make_unique<ck4a::CaptureManager>();
		mCapture->init( captureInfo );
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to reload (capture)", exc );
		CI_LOG_E( "stacktrace:\n" << ma::stackTraceAsString( 0, 7 ) );
	}

	//try {
	//	auto gestureInfo = ma::config()->get<ma::Info>( "gestures" );
	//	mGestures = make_unique<ck4a::GestureManager>();
	//	mGestures->init( gestureInfo );
	//}
	//catch( exception &exc ) {
	//	CI_LOG_EXCEPTION( "failed to reload (gestures)", exc );
	//	CI_LOG_E( "stacktrace:\n" << ma::stackTraceAsString( 0, 7 ) );
	//}

	try {
		auto sceneInfo = ma::config()->get<ma::Info>( "scene" );
		mScene = make_unique<DebugScene>();
		mScene->load( sceneInfo, fullReload );
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to reload (scene)", exc );
	}
}

void AzureKinectTestApp::save( bool full )
{
	auto fullPath = app::getAssetPath( "" ) / CONFIG_FILES[2];

	try {
		Timer timer( true );

		ma::Info info;
		save( info, full );

		ofstream ostream( fullPath.c_str() );
		ostream << info;
		ostream.close();

		CI_LOG_I( "saved user config to file: " << fullPath << " in " << timer.getSeconds() << " seconds." );
		ma::notifyResourceReloaded();
	}
	catch( std::exception &exc ) {
		CI_LOG_EXCEPTION( "Failed to write archive", exc );
	}
}


void AzureKinectTestApp::save( ma::Info &info, bool full ) const
{
	if( mScene ) {
		ma::Info sceneInfo;
		mScene->save( sceneInfo );
		info["scene"] = sceneInfo;
	}

	if( mCapture ) {
		ma::Info captureInfo;
		mCapture->save( captureInfo );
		info["capture"] = captureInfo;
	}
}

void AzureKinectTestApp::keyDown( app::KeyEvent event )
{
	if( event.isControlDown() ) {
		if( tolower( event.getChar() ) == 'r' ) {
			reload( event.isShiftDown() );
		}
		else if( tolower( event.getChar() ) == 's' ) {
			save( event.isShiftDown() );
		}
		else if( event.getChar() == 'f' ) {
			setFullScreen( ! isFullScreen() );
		}
		//else if( event.getChar() == 'c' ) {
		//	initCam();
		//}
		else if( event.getChar() == 'v' ) {
			gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
		}
		else if( event.getChar() == 'g' ) {
			mImGuiEnabled = ! mImGuiEnabled;
			CI_LOG_I( "ImGui enabled: " << mImGuiEnabled );
		}
		else if( event.getChar() == 'm' ) {
			//mAutoHideMouse = ! mAutoHideMouse;
			//CI_LOG_I( "auto-hide mouse: " << mAutoHideMouse );
			//mTimerMouse = Timer( true );
			mMouseHidden = ! mMouseHidden;
			updateMouseHidden();
		}
		else if( event.getChar() == 'q' ) {
			CI_LOG_I( "Ctrl + q: quitting app." );
			quit();
		}
	}
	else {
		if( mCapture ) {
			mCapture->keyDown( event );
		}
	}
}

void AzureKinectTestApp::keyUp( app::KeyEvent event )
{
}

void AzureKinectTestApp::onMouseDown( app::MouseEvent &event )
{
}

void AzureKinectTestApp::onMouseDrag( app::MouseEvent &event )
{
}	

void AzureKinectTestApp::onMouseUp( app::MouseEvent &event )
{
}

void AzureKinectTestApp::onMouseWheel( app::MouseEvent &event )
{
}

void AzureKinectTestApp::mouseMove( app::MouseEvent event )
{
}


void AzureKinectTestApp::resize()
{
}

void AzureKinectTestApp::updateMouseHidden()
{
	if( mMouseHidden ) {
		hideCursor();
	}
	else {
		showCursor();
	}
}

void AzureKinectTestApp::update()
{
#if 0
	// testing catching a SIGFPE / divide-by-zero
	//raise( SIGFPE );

	try {
		volatile int b = 1;
		volatile int c = 0;
		volatile int d = b / c;
	}
	//catch( divisionbyzero_exception& ex ) {
	//	CI_LOG_EXCEPTION( "bam", ex );
	//}
	catch( ... ) {
		CI_LOG_F( "unknown exception caught" );
	}
#endif

#if 0
	// TODO: produce and try to handle a nullptr access
	try {
		ck4a::Body *blah = nullptr;
		string id = blah->mId;
		CI_LOG_I( "id: " << id );
	}
	catch( ... ) {
		CI_LOG_F( "unknown exception caught. " );
		CI_LOG_E( "stacktrace:\n" << ma::stackTraceAsString( 0, 20 ) ); // in the case above, I think this goes down because I used string above
		std::exit( EXIT_FAILURE );
	}
#endif

	if( mCapture ) {
		CI_PROFILE_CPU( "update - capture" );
		mCapture->update();
	}

	if( mMergeBodiesEnabled ) {
		CI_PROFILE_CPU( "update - merge bodies" );
		mergeBodies();
	}

	//if( mGestures && mGestures->isEnabled() ) {
	//	CI_PROFILE_CPU( "update - gestures" );
	//	for( const auto &mp : mMergedBodies ) {
	//		mGestures->processBody( mp.second );
	//	}

	//	mGestures->update();
	//}

	if( mImGuiEnabled ) {
		updateImGui();
	}
}

void AzureKinectTestApp::draw()
{
	gl::clear( Color::gray( 0.1f ) );

	if( mCapture && mScene ) {
		mScene->draw( mCapture.get() );
	}

	if( mProfilingUIEnabled ) {
		imx::Profiling( &mProfilingUIEnabled );
	}
}

// ----------------------------------------------------------------------------------------------------
// Scene
// ----------------------------------------------------------------------------------------------------
// TODO NEXT: move this all to DebugScene


#define DEBUG_BODY_UI 0

// TODO: trying to figure out how to use this wrapper to avoid all of the #if DEBUG_BODY_UI sections in the code below
//#if DEBUG_BODY_UI
//#define DEBUG_BODY ( (void)( ... ) )
//
//#else
//#define DEBUG_BODY ( (void)( 0 ) )
//#endif

std::string makeMergedBodyKey( const std::string &deviceId, const std::string bodyId )
{
	return deviceId + "-" + bodyId;
}

void AzureKinectTestApp::mergeBodies()
{
	if( ! mCapture ) {
		return;
	}

	mResolveJoints = {
		ck4a::JointType::Head,
	};

	// TODO: use more complete set
	//mResolveJoints = {
	//	ck4a::JointType::Head,
	//	ck4a::JointType::SpineNavel,
	//	ck4a::JointType::SpineChest,
	//	ck4a::JointType::Neck
	//};

	map<string, ck4a::Body> matchedBodies; // 

	// match up the bodiesA from multiple cameras
	// - their current ids don't mean anything, will have to asign new ones
	// - only compare bodiesA to thoe from other devices

#if DEBUG_BODY_UI
	im::Begin( "Debug Resolve Bodies" );
#endif

	for( int deviceIndex = 0; deviceIndex < mCapture->getNumDevices(); deviceIndex++ ) {
		const auto &device = mCapture->getDevices()[deviceIndex];
		if( ! device->isEnabled() ) {
			continue;
		}

		auto bodiesA = device->getBodies();

#if DEBUG_BODY_UI
		im::BulletText( "device index: %d, bodies: %d", deviceIndex , bodiesA.size() );
		im::Indent();
#endif

		for( const auto &bodyA : bodiesA ) {
			// TODO: for loop over mResolveJoints
			// - but could also just say if the heads are far enough apart, don't consider it all
			//    - this effectively binning the bodies relative to their head
			//    - or use the body's center pos
			const ck4a::Joint *jointA = bodyA.getJoint( ck4a::JointType::Head );
			// if we have the main jointA, compare it to those in matchedBodies
			if( jointA && (int)jointA->mConfidence >= (int)ck4a::JointConfidence::Medium ) {
				// if we're on the first device, just add these bodiesA to the map
				if( deviceIndex == 0 ) {
					string key = makeMergedBodyKey( device->getId(), bodyA.getId() );
					ck4a::Body bodyCopy = bodyA;
					bodyCopy.mId = key;
					auto resultIt = matchedBodies.insert( { key, bodyCopy } );
					CI_VERIFY( resultIt.second );

#if DEBUG_BODY_UI
					im::BulletText( "added body with key: %s", key.c_str() );
#endif
				}
				else {
					// compare this device's bodies to those in matchedBodies
					bool bodyMatched = false;
					for( auto &mp : matchedBodies ) {
						auto &bodyM = mp.second;
#if DEBUG_BODY_UI
						im::BulletText( "bodyA: %d : bodyM: %d", bodyA.getId(), bodyM.getId() );
						im::SameLine();
#endif
						// compare joint and if close enough, merge
						const ck4a::Joint *jointB = bodyM.getJoint( ck4a::JointType::Head );
						CI_ASSERT( (int)jointB->mConfidence >= (int)ck4a::JointConfidence::Medium );

						float dist = glm::distance( jointA->mPos, jointB->mPos );
						Color col( 1, 1, 1 );
						if( dist < mJointDistanceConsideredSame ) {
							col = Color( 0, 1, 0 );
						}
#if DEBUG_BODY_UI
						im::TextColored( col, "- dist: %0.3f", dist );
#endif

						if( dist < mJointDistanceConsideredSame ) {
							bodyMatched = true;
							// merge the two bodies
							// TODO: consider storing indices + devices and doing the merge afterwards
							bodyM.merge( bodyA );
#if DEBUG_BODY_UI
							im::SameLine(); im::Text( "|merged|" );
#endif
							break; // don't compare any other bodies from this device
							// TODO: we might want to find the closest / best candidate body instead
						}

					}

					// if we couldn't find a match from a device other than the first, add new body
					if( ! bodyMatched ) {
						string key = makeMergedBodyKey( device->getId(), bodyA.getId() );
						ck4a::Body bodyCopy = bodyA;
						bodyCopy.mId = key;
						auto resultIt = matchedBodies.insert( { key, bodyCopy } );
						CI_VERIFY( resultIt.second );
#if DEBUG_BODY_UI
						im::Indent();
						im::BulletText( "no match, added body with id: %d", bodyA.getId() );
						im::Unindent();
#endif
					}
				}
			}
			else {
				// main jointA not present
				// TODO: disregard this bodyA as a candidate?

			}
		}
#if DEBUG_BODY_UI
		im::Unindent();
#endif
	}

	// if a body isn't matched, mark it as removed from GestureManager
	// - may rework this later based on timestamps or something
	//if( mGestures && mGestures->isEnabled() ) {
	//	for( const auto &mp : mMergedBodies ) {
	//		if( matchedBodies.count( mp.first ) == 0 ) {
	//			mGestures->removeBody( mp.second );
	//		}
	//	}
	//}

	mMergedBodies = matchedBodies;

#if DEBUG_BODY_UI

	im::Separator();
	im::Text( "total merged bodies: %d", mMergedBodies.size() );

	im::End();
#endif
}

// ----------------------------------------------------------------------------------------------------
// ImGui
// ----------------------------------------------------------------------------------------------------

// called on first render loop, because we need a valid gl::context
void AzureKinectTestApp::initImGui()
{
	im::Initialize();
	im::StyleColorsDark();

	im::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

// only called from main thread, after ImGui has been initialized on the first Window's render thread
void AzureKinectTestApp::updateImGui()
{
	CI_PROFILE( "ImGui update" );

	IMGUI_CHECKVERSION();

	{
		ImGuiCond flags = ImGuiCond_FirstUseEver;
		//ImGuiCond flags = ImGuiCond_Always;

		const float spacing = 4;
		const float collapsedHeight = 20;
		vec2 winPos = { 10, 10 };
		vec2 winSize = { 322, 155 };
		im::SetWindowPos( "General", winPos, flags );
		im::SetWindowSize( "General", winSize, flags );

		winPos.y += winSize.y + spacing;
		winSize.y = 400;
		im::SetWindowPos( "Scene", winPos, flags );
		im::SetWindowSize( "Scene", winSize, flags );
	}

	imx::SetNotificationColors();

	im::GetStyle().WindowRounding = 4.0f;
	//im::GetStyle().Alpha = 0.85f;

	//im::GetStyle().Colors[ImGuiCol_Text] = Color( 0.187f, 1, 0.269f );
	im::GetStyle().Colors[ImGuiCol_Text] = Color( 0.187f, 1, 1 );

	im::GetIO().FontGlobalScale = getWindowContentScale();
	im::GetIO().FontAllowUserScaling = true;

	//im::GetIO().ConfigWindowsMoveFromTitleBarOnly = false;
	//im::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	if( im::Begin( "General", nullptr ) ) {
		im::Text( "fps: %0.2f, seconds running: %0.1f", app::App::get()->getAverageFps(), app::App::get()->getElapsedSeconds() );
		im::Separator();
		//im::SameLine(); im::Text( "(ctrl + s)" );

		if( im::Button( "reload" ) ) {
			reload( false );
		}
		if( im::IsItemHovered() ) {
			im::SetTooltip( "full: hold shift" );
		}
		im::SameLine();
		im::Text( "(ctrl + r)" );
		if( im::Button( "save user settings" ) ) {
			save( false );
		}
		if( im::IsItemHovered() ) {
			im::SetTooltip( "full: hold shift" );
		}
		im::SameLine();
		im::Text( "(ctrl + s)" );

		im::Checkbox( "GUI (ctrl + g)", &mImGuiEnabled );

		if( im::Checkbox( "Hide mouse (ctrl + m)", &mMouseHidden ) ) {
			updateMouseHidden();
		}

		im::Checkbox( "Logs", &mLogsUIEnabled );
		im::Checkbox( "Profiling", &mProfilingUIEnabled );
		if( mCapture ) {
			mCapture->enabledUI();
		}
	
		if( im::CollapsingHeader( "dev" ) ) {

			//im::DragFloat4( "dev params", &mDevParams.x, 0.2f );

			static bool showImGuiDemo = false;
			im::Checkbox( "ImGui Demo", &showImGuiDemo );
			if( showImGuiDemo ) {
				im::ShowDemoWindow( &showImGuiDemo );
			}

			im::Separator();

			im::Text( "gpu vendor: %s\ndriver version: %s", gl::getVendorString().c_str(), gl::getVersionString().c_str() );

			if( im::TreeNode( "asset directories" ) ) {
				for( const auto &dir : app::getAssetDirectories() ) {
					im::BulletText( "%s", dir.string().c_str() );
				}
				im::TreePop();
			}
		}

	}
	im::End(); // "General"

	if( mCapture ) {
		mCapture->updateUI();
	}

	//if( mGestures ) {
	//	mGestures->updateUI();
	//}

	if( mScene ) {
		mScene->updateUI();
	}

	if( mLogsUIEnabled ) {
		imx::Logs( "Logs", &mLogsUIEnabled );
	}
}

// ----------------------------------------------------------------------------------------------------
// CINDER_APP
// ----------------------------------------------------------------------------------------------------

#if DEBUG_GL
CINDER_APP( AzureKinectTestApp, app::RendererGl( app::RendererGl::Options().msaa( MSAA ).debugBreak().debug() ), prepareSettings )
#else
CINDER_APP( AzureKinectTestApp, RendererGl( RendererGl::Options().msaa( MSAA ) ), prepareSettings )
#endif
