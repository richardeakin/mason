
#include "mason/Config.h"
#include "cinder/Utilities.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"
#include "cinder/app/App.h"

#include "jsoncpp/json.h"

using namespace ci;
using namespace std;

namespace mason {

namespace detail {

static ma::Info sConfig;

void setConfig( const ma::Info &config )
{
	sConfig = config;
}

} // namespace mason::detail

void loadConfig( const fs::path &filename, const fs::path &cascadingFilename )
{
	loadConfig( vector<fs::path>{ filename, cascadingFilename } );
}

void loadConfig( const vector<fs::path> &cascadingFilenames )
{
	if( cascadingFilenames.empty() ) {
		CI_LOG_E( "no config filenames specified." );
		return;
	}

	// load main config file
	fs::path mainFilename = cascadingFilenames.front();
	ma::Info config;
	try {
		config = ma::Info::convert<Json::Value>( app::loadAsset( mainFilename ) );
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load config file: " << mainFilename, exc );
	}

	// merge any cascading config files
	if( cascadingFilenames.size() > 1 ) {
		for( auto &fIt = cascadingFilenames.begin() + 1; fIt != cascadingFilenames.end(); ++fIt ) {
			auto fullPath = app::getAssetPath( *fIt );
			try {
				if( ! fullPath.empty() ) {
					auto j = ma::Info::convert<Json::Value>( loadFile( fullPath ) );
					config.merge( j );
				}
			}
			catch( exception &exc ) {
				CI_LOG_EXCEPTION( "failed to load config file: " << fullPath, exc );
			}
		}
	}

	//CI_LOG_I( "config (final):\n" << config );
	ma::detail::setConfig( config );
}

ma::Info*	config()
{
	return &detail::sConfig;
}

} // namespace mason
