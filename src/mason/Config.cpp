
#include "mason/Config.h"
#include "cinder/Utilities.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"

using namespace ci;
using namespace std;

namespace mason {

// static
Config* Config::instance()
{
	static Config sInstance;
	return &sInstance;
}

bool Config::read( const DataSourceRef &source )
{
	mSource = source;

	if( mSource->isFilePath() ) {
		mTarget = DataTargetPath::createRef( mSource->getFilePath() );
	}
	else {
		// We can only write to files on disk.
		mTarget.reset();
	}

	try {
		string data = loadString( source );
	
		Json::Reader reader;
		bool success = reader.parse( data, mRoot );
		if( ! success ) {
			string errorMessage = reader.getFormattedErrorMessages();
			throw ConfigExc( "Json::Reader failed to parse source, error message: " + errorMessage );
		}

		mDirty = false;
	}
	catch( std::exception &exc ) {
		CI_LOG_EXCEPTION( "failed to read source", exc );
		return false;
	}

	return true;
}

bool Config::write( const DataTargetRef &target )
{
	if( target ) {
		const fs::path &path = target->getFilePath();
		if( ! fs::exists( path ) ) {
			fs::create_directories( path.parent_path() );
			mDirty = true;
		}

		if( mDirty ) {
			Json::StyledWriter writer;
			string data = writer.write( mRoot );

			ofstream ostream( path.c_str() );
			ostream << data;
			ostream.close();
		}

		return true;
	}

	return false;
}

fs::path Config::getTargetFilePath() const
{
	if( ! mTarget )
		return {};

	return mTarget->getFilePath();
}

} // namespace mason
