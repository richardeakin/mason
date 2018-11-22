
#include "mason/Config.h"
#include "cinder/Utilities.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"
#include "cinder/app/App.h"

using namespace ci;
using namespace std;

namespace mason {

namespace detail {

static ma::Dictionary sConfig;

void setConfig( const ma::Dictionary &config )
{
	sConfig = config;
}

} // namespace mason::detail

void loadConfig( const ci::fs::path &filename )
{
	try {
		// load main config
		auto config = ma::Dictionary::convert<Json::Value>( app::loadAsset( filename ) );

		//CI_LOG_I( "config (merged):\n" << config );
		ma::detail::setConfig( config );
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load config file: " << filename, exc );
	}
}

ma::Dictionary*	config()
{
	return &detail::sConfig;
}

} // namespace mason
