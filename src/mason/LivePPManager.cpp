#include "mason/LivePPManager.h"
#include "mason/Notifications.h"
#include "cinder/Log.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/app/App.h"

#include "../../thirdparty/LivePP/API/LPP_ForceLinkStaticRuntime.h"
#include "../../thirdparty/LivePP/API/LPP_API.h"


using namespace ci;
using namespace std;

namespace mason {

// static
LivePPManager* LivePPManager::instance()
{
	static LivePPManager sInstance;
	return &sInstance;
}

LivePPManager::LivePPManager()
{
}

ci::signals::Connection LivePPManager::connectPrePatch( const std::function<void ()> &fn )
{
	return instance()->getSignalPrePatch().connect( fn );
}

ci::signals::Connection LivePPManager::connectPostPatch( const std::function<void ()> &fn )
{
	return instance()->getSignalPostPatch().connect( fn );
}

bool initLivePP( const fs::path &LivePPPath, const std::string groupName )
{
	HMODULE livePPModule = lpp::lppLoadAndRegister( msw::toWideString( LivePPPath.string() ).c_str(), groupName.c_str() );
	if( ! livePPModule ) {
		auto appPath = app::Platform::get()->getExecutablePath();
		CI_LOG_E( "failed to load LivePP Module specified with relative folder: " << LivePPPath << ". Path should be relative to .vcxproj folder. executable path: " << appPath );
		return false;
	}

	lpp::lppEnableAllCallingModulesSync( livePPModule );
	return true;
}

void onLppCompileSuccess()
{
	ma::NotificationCenter::post( ma::NOTIFY_RESOURCE_RELOADED );
}

void onLppCompileError()
{
	ma::NotificationCenter::post( ma::NOTIFY_ERROR );
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
