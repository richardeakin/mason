#include "mason/extra/LivePPManager.h"
//#include "mason/Notifications.h"
#include "cinder/Log.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/app/App.h"

// note: add LivePP/API to your include path
#if ! defined( CINDER_SHARED )
#include "LPP_ForceLinkStaticRuntime.h"
#endif

#include "LPP_API.h"

using namespace ci;
using namespace std;

namespace mason {

namespace {

HMODULE	sLivePPModule = nullptr;

} // anonymous namespace

bool initLivePP( const fs::path &LivePPPath, const std::string &groupName )
{	
	return LivePPManager::instance()->initLivePP( LivePPPath, groupName );
}

// static
LivePPManager* LivePPManager::instance()
{
	static LivePPManager sInstance;
	return &sInstance;
}

LivePPManager::LivePPManager()
{
}

bool LivePPManager::initLivePP( const fs::path &LivePPPath, const std::string &groupName )
{	
	sLivePPModule = lpp::lppLoadAndRegister( msw::toWideString( LivePPPath.string() ).c_str(), groupName.c_str() );
	if( ! sLivePPModule ) {
		auto appPath = app::Platform::get()->getExecutablePath();
		CI_LOG_E( "failed to load LivePP Module specified with relative folder: " << LivePPPath << ". Path should be relative to .vcxproj folder. executable path: " << appPath );
		return false;
	}

	lpp::lppEnableAllCallingModulesSync( sLivePPModule ); // TODO: try lppEnableAllCallingModulesAsync

	// connect update loop to lppSyncPoint
	auto app = ci::app::App::get();
	if( app ) {
		mConnUpdate = app->getSignalUpdate().connect( [this]() { lpp::lppSyncPoint( sLivePPModule ); } );
	}

	return true;
}

ci::signals::Connection LivePPManager::connectPrePatch( const std::function<void ()> &fn )
{
	return instance()->getSignalPrePatch().connect( fn );
}

ci::signals::Connection LivePPManager::connectPostPatch( const std::function<void ()> &fn )
{
	return instance()->getSignalPostPatch().connect( fn );
}


void onLppCompileSuccess()
{
	//ma::NotificationCenter::post( ma::NOTIFY_RESOURCE_RELOADED );
}

void onLppCompileError()
{
	//ma::NotificationCenter::post( ma::NOTIFY_ERROR );
}

void onLppPrePatch()
{
	app::App::get()->dispatchAsync( [] {
		LivePPManager::instance()->getSignalPrePatch().emit();
	} );
}

void onLppPostPatch()
{
	app::App::get()->dispatchAsync( [] {
		LivePPManager::instance()->getSignalPostPatch().emit();
	} );
}

} // namespace mason


LPP_COMPILE_SUCCESS_HOOK( mason::onLppCompileSuccess );
LPP_COMPILE_ERROR_HOOK( mason::onLppCompileError );
LPP_PREPATCH_HOOK( mason::onLppPrePatch );
LPP_POSTPATCH_HOOK( mason::onLppPostPatch );