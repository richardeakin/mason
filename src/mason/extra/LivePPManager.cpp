// note: add LivePP/API to your include path

#include "mason/extra/LivePPManager.h"
//#include "mason/Notifications.h" // TODO: move this somewhere outside of this file so it's not a depedency
#include "cinder/Log.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/app/App.h"

#if ! defined( CINDER_SHARED )
// You're supposed to include this to cover some edge cases, but it causes compile errors with latest v142 toolset.
// note from author: "In 99% of cases you won't need it and you'll probably be fine not including it (LPP_ForceLinkStaticRuntime.h)
//#include "LPP_ForceLinkStaticRuntime.h"
#endif

//#include "LPP_API.h"
#include "LPP_API_x64_CPP.h"


using namespace ci;
using namespace std;

namespace mason {

namespace {

//HMODULE	sLivePPModule = nullptr;

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

// TODO: should probably use the synchronized agent as explained here https://liveplusplus.tech/docs/documentation.html#creating_synchronized_agent
bool LivePPManager::initLivePP( const fs::path &LivePPPath, const std::string &groupName )
{
#if 0
	sLivePPModule = lpp::lppLoadAndRegister( msw::toWideString( LivePPPath.string() ).c_str(), groupName.c_str() );
	if( ! sLivePPModule ) {
		auto appPath = app::Platform::get()->getExecutablePath();
		CI_LOG_E( "failed to load LivePP Module specified with relative folder: " << LivePPPath << ". Path should be relative to .vcxproj folder. executable path: " << appPath );
		return false;
	}

	lpp::lppEnableAllCallingModulesSync( sLivePPModule ); // TODO: try lppEnableAllCallingModulesAsync

#else
	if( ! fs::exists( LivePPPath ) ) {
		auto appPath = app::Platform::get()->getExecutablePath();
		CI_LOG_E( "Cannot find LivePP folder at expected path: " << LivePPPath << ". Path should be relative to .vcxproj folder. executable path: " << appPath );
		return false;
	}
	lpp::LppDefaultAgent lppAgent = lpp::LppCreateDefaultAgent( msw::toWideString( LivePPPath.string() ).c_str() );
	lppAgent.EnableModule( lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES );

	// TODO: move to an optional public method
	// destroy the Live++ agent
	//lpp::LppDestroyDefaultAgent(&lppAgent);

#endif

	// TODO: re-enable
#if 0

	// connect update loop to lppSyncPoint
	auto app = ci::app::App::get();
	if( app ) {
		mConnUpdate = app->getSignalUpdate().connect( [this]() { lpp::lppSyncPoint( sLivePPModule ); } );
	}
#endif

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

#if 0
LPP_COMPILE_SUCCESS_HOOK( mason::onLppCompileSuccess );
LPP_COMPILE_ERROR_HOOK( mason::onLppCompileError );
LPP_PREPATCH_HOOK( mason::onLppPrePatch );
LPP_POSTPATCH_HOOK( mason::onLppPostPatch );
#endif
