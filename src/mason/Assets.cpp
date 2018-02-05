/*
Copyright (c) 2015, The Barbarian Group
All rights reserved.

This code is designed for use with the Cinder C++ library, http://libcinder.org

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "mason/Assets.h"
#include "mason/Common.h"

#include "cinder/Log.h"
#include "cinder/Breakpoint.h"
#include "cinder/app/App.h"
#include "cinder/Utilities.h"

using namespace ci;
using namespace std;

namespace mason {

namespace {


//! Returns a unique ID based on the supplied string.
uint32_t makeUuid( const string &str )
{
	hash<string> hasher;
	return uint32_t( hasher( str ) );
}

//! Returns a unique ID based on the supplied string.
uint32_t makeUuid( const string &str, const gl::GlslProg::Format &format )
{
	hash<string> hasher;

	if( format.getDefines().empty() ) {
		return uint32_t( hasher( str ) );
	}
	else {
		// append defines to hash so that it is unique
		auto strWithDefines = str;
		for( const auto &define : format.getDefines() ) {
			strWithDefines += define.first + define.second;
		}
		return uint32_t( hasher( strWithDefines ) );
	}
}

void setShaderFilePathBySuffix( const DataSourceRef &shaderFile, gl::GlslProg::Format *format )
{
	string suffix = shaderFile->getFilePathHint().extension().string();

	if( suffix == ".vert" )
		format->vertex( shaderFile );
	else if( suffix == ".frag" )
		format->fragment( shaderFile );
#if defined( CINDER_GL_HAS_COMPUTE_SHADER )
	else if( suffix == ".comp" )
		format->compute( shaderFile );
#endif
	else if( suffix == ".geom" )
		format->geometry( shaderFile );
	else if( suffix == ".tesc" )
		format->tessellationCtrl( shaderFile );
	else if( suffix == ".tese" )
		format->tessellationEval( shaderFile );
	else {
		string errorMessage = "unexpected suffix '" + suffix + "' for shader file: " + shaderFile->getFilePathHint().string();
		throw AssetManagerExc( errorMessage );
	}
}

} // anonymous namespace

// ----------------------------------------------------------------------------------------------------
// AssetManager
// ----------------------------------------------------------------------------------------------------

// static
AssetManager* AssetManager::instance()
{
	static AssetManager sInstance;
	return &sInstance;
}

AssetManager::AssetManager()
{
}

AssetManager::~AssetManager()
{
}

ci::signals::Connection AssetManager::getShader( const fs::path &vertex, const gl::GlslProg::Format &format, const function<void( gl::GlslProgRef )> &updateCallback )
{
	uint32_t hash = makeUuid( vertex.generic_string(), format );

	auto glslModifiedCallback = [this, format, hash, updateCallback, vertex] {
		try {
			// Update the format's source.
			DataSourceRef shaderDataSource = findFile( vertex );

			// need to make an additional copy of format because the one captured by value in the lambda is const
			auto formatCopy = format;
			setShaderFilePathBySuffix( shaderDataSource, &formatCopy );

			auto group = getAssetGroupRef( hash );

			auto glsl = mShaders[hash].lock();
			if( ! glsl || mGroups[hash]->isModified() ) {
				glsl = reloadShader( formatCopy, group, hash );
				notifyResourceReloaded();
			}

			if( updateCallback )
				updateCallback( glsl );
		}
		catch( const exception &exc ) {
			if( mAssetErrors.count( hash ) == 0 ) {
				mAssetErrors[hash] = true;
				CI_LOG_EXCEPTION( "Failed to reload glsl: [" << vertex.filename() << "]", exc );
			}
		}
	};

	// add a callback to this group that will fire when the shader is modified
	auto group = getAssetGroupRef( hash );
	auto conn = group->addModifiedCallback( glslModifiedCallback );

	// ensure the callback specific to this request is fired on initial request
	glslModifiedCallback();
	return conn;
}

// TODO: make this generic, vector of fs::paths along with overloads
ci::signals::Connection AssetManager::getShader( const fs::path &vertex, const fs::path &fragment, const gl::GlslProg::Format &format, const function<void( gl::GlslProgRef )> &updateCallback )
{
	uint32_t hash = makeUuid( vertex.generic_string() + fragment.generic_string(), format );

	auto glslModifiedCallback = [this, format, hash, updateCallback, vertex, fragment] {
		try {

			// Update the format's source.
			DataSourceRef vertFile = findFile( vertex );
			DataSourceRef fragFile = findFile( fragment );

			// need to make an additional copy of format because the one captured by value in the lambda is const
			auto formatCopy = format;
			formatCopy.vertex( vertFile );
			formatCopy.fragment( fragFile );

			auto group = getAssetGroupRef( hash );

			auto glsl = mShaders[hash].lock();
			if( ! glsl || mGroups[hash]->isModified() ) {
				glsl = reloadShader( formatCopy, group, hash );
				notifyResourceReloaded();
			}

			if( updateCallback )
				updateCallback( glsl );
		}
		catch( const exception &exc ) {
			if( mAssetErrors.count( hash ) == 0 ) {
				mAssetErrors[hash] = true;
				CI_LOG_EXCEPTION( "Failed to reload glsl: [" << vertex.filename() << "," << fragment.filename() << "]", exc );
			}
		}
	};

	// add a callback to this group that will fire when the shader is modified
	auto group = getAssetGroupRef( hash );	
	auto conn = group->addModifiedCallback( glslModifiedCallback );

	// ensure the callback specific to this request is fired on initial request
	glslModifiedCallback();
	return conn;
}

void AssetManager::initShaderPreprocessorLazy()
{
	if( mShaderPreprocessor )
		return;
		
	mShaderPreprocessor = make_unique<gl::ShaderPreprocessor>();

#if defined( CINDER_GL_ES )
	mShaderPreprocessor->setVersion( 300 );
	mShaderPreprocessor->addDefine( "CINDER_GL_ES" );
#else
	mShaderPreprocessor->setVersion( 410 );
	mShaderPreprocessor->addDefine( "CINDER_GL_CORE" );
#endif

#if defined( CINDER_MSW )
	auto vendor = gl::getVendorString();
	std::transform( vendor.begin(), vendor.end(), vendor.begin(), tolower );
	if( vendor.find( "nvidia" ) != string::npos ) {
		mShaderPreprocessor->setUseFilenameInLineDirectiveEnabled( true );
	}
#endif
}

ci::gl::GlslProgRef AssetManager::reloadShader( ci::gl::GlslProg::Format &format, const AssetGroupRef &group, uint32_t hash )
{
	initShaderPreprocessorLazy();

	// we use our own preprocessor and pull out included files to watch them at each stage.
	const auto formatDefines = format.getDefines();
	format.preprocess( false );

	// Add defines specified to GlslProg::Format, for just this shader
	// TODO: this could blow away existing defines, and then removing them later would be a problem
	// - solution would be that ShaderPreprocessor can take a per-parse set of defines too
	for( const auto &define : formatDefines ) {
		mShaderPreprocessor->addDefine( define.first, define.second );
	}

	std::vector<std::pair<ci::fs::path, std::string>>	sources;

	std::vector<fs::path> includedFiles;
	std::set<fs::path> stageIncludedFiles; // '#include'd files parsed by the preprocessor during each stage

	// copy shader paths out first, resetting source on GlslProg::Format will clear them

	// always add the asset paths and clear modified indicator, which enables the shader to be reloaded even if it errored on initial load
	group->setModified( false );
	if( ! format.getVertexPath().empty() ) {
		auto shaderPath = format.getVertexPath();
		group->addAsset( getAssetRef( shaderPath ) );

		string parsedShader = mShaderPreprocessor->parse( format.getVertex(), shaderPath, &stageIncludedFiles );
		format.vertex( parsedShader );
		sources.push_back( { shaderPath, parsedShader } );
		includedFiles.insert( includedFiles.end(), stageIncludedFiles.begin(), stageIncludedFiles.end() );
	}
	if( ! format.getFragmentPath().empty() ) {
		auto shaderPath = format.getFragmentPath();
		group->addAsset( getAssetRef( shaderPath ) );

		stageIncludedFiles.clear();
		string parsedShader = mShaderPreprocessor->parse( format.getFragment(), shaderPath, &stageIncludedFiles );
		format.fragment( parsedShader );
		sources.push_back( { shaderPath, parsedShader } );
		includedFiles.insert( includedFiles.end(), stageIncludedFiles.begin(), stageIncludedFiles.end() );
	}
#if defined( CINDER_GL_HAS_COMPUTE_SHADER )
	if( ! format.getComputePath().empty() ) {
		auto shaderPath = format.getComputePath();
		group->addAsset( getAssetRef( shaderPath ) );

		stageIncludedFiles.clear();
		string parsedShader = mShaderPreprocessor->parse( format.getCompute(), shaderPath, &stageIncludedFiles );
		format.compute( parsedShader );
		sources.push_back( { shaderPath, parsedShader } );
		includedFiles.insert( includedFiles.end(), stageIncludedFiles.begin(), stageIncludedFiles.end() );
	}
#endif
	// all included files before trying to create the shader, so that in the case of an error we are still watching those files too.
	for( const auto &includeFile : includedFiles ) {
		group->addAsset( getAssetRef( includeFile ) );
	}

	// remove all defines specified by Format (see TODO above)
	for( const auto &define : formatDefines ) {
		mShaderPreprocessor->removeDefine( define.first );
	}

	auto shader = gl::GlslProg::create( format );
	mShaders[hash] = shader;
	mAssetErrors.erase( hash );
	mSignalShaderLoaded.emit( shader, sources );

	return shader;
}

signals::Connection AssetManager::getTexture( const ci::fs::path &texturePath, const std::function<void( ci::gl::Texture2dRef )> &updateCallback  )
{
	uint32_t hash = makeUuid( texturePath.generic_string() );

	auto textureModifiedCallback = [this, texturePath, hash, updateCallback] {
		try {
			auto dataSource = findFile( texturePath );

			gl::Texture2dRef texture = mTextures[hash].lock();
			if( ! texture || mGroups[hash]->isModified() ) {
				Surface surface = loadImage( dataSource );

#if USE_DEEP_LOADING
				if( texture && texture->getSize() == surface.getSize() ) {
					texture->update( surface, 0 );
				}
				else
#endif
				{
					gl::Texture2d::Format format = gl::Texture2d::Format().mipmap( true ).minFilter( GL_LINEAR_MIPMAP_LINEAR ).wrap( GL_REPEAT );
					texture = gl::Texture2d::create( surface, format );
					mTextures[hash] = texture;
				}


				notifyResourceReloaded();
				mAssetErrors.erase( hash );

				if( updateCallback )
					updateCallback( texture );


			}
		}
		catch( const exception &exc ) {
			if( mAssetErrors.count( hash ) == 0 ) {
				mAssetErrors[hash] = true;
				CI_LOG_EXCEPTION( "Failed to reload texture: [" << texturePath.filename() << "]", exc );
			}
		}
	};

	auto group = getAssetGroupRef( hash );
	group->addAsset( getAssetRef( texturePath ) );
	group->setModified( false );

	auto connection = group->addModifiedCallback( textureModifiedCallback );

	// ensure the callback specific to this request is fired on initial request
	textureModifiedCallback();
	return connection;
}

ci::signals::Connection AssetManager::getFile( const fs::path &path, const std::function<void( DataSourceRef )> &updateCallback )
{
	try {
#if defined( MASON_DEPLOY ) || defined( CINDER_ANDROID )
		auto assetFile = findFile( path );
		updateCallback( assetFile );
		return ma::WatchRef();
#else
		auto conn = FileWatcher::instance().watch( path, [updateCallback]( const WatchEvent &event ) {
			updateCallback( loadFile( event.getFile() ) );
		} );

		return conn;
#endif
	}
	catch( std::exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load file for path: " << path, exc );
		CI_LOG_E( "stacktrace:\n" << ma::stackTraceAsString( 0, 7 ) );
	}

	return {};
}

ci::DataSourceRef AssetManager::loadAsset( const ci::fs::path &path )
{
	return findFile( path );
}

void AssetManager::enableLiveAssets( bool enabled )
{
	// TODO: use our own FileWatcher instance
	FileWatcher::instance().setWatchingEnabled( enabled );
}

bool AssetManager::isLiveAssetsEnabled() const
{
	return true;
}

void AssetManager::clear()
{
	CI_LOG_I( "Clearing: " << mGroups.size() << " groups, " << mAssets.size() << " assets, " << mShaders.size() << " shaders, " <<  mTextures.size() << " textures." );

	mGroups.clear();
	mAssets.clear();
	mAssetIds.clear();
	mShaders.clear();
	mTextures.clear();
}

void AssetManager::getFilesInUse( vector<fs::path> *paths ) const
{
//	lock_guard<std::mutex> lock( mAssetsLock );

	for( auto itr = mAssets.begin(); itr != mAssets.end(); ++itr ) {
		const auto &asset = itr->second;

		if( ! isLiveAssetsEnabled() && asset->isInUse() ) {
			// Make sure the assets is indeed in use.
			asset->setInUse( false );
			for( auto &gref : asset->mGroups ) {
				auto group = gref.lock();
				if( group ) {
					asset->setInUse( true );
					break;
				}
			}

			if( asset->isInUse() )
				paths->push_back( asset->getPath() );
		}
	}
}

AssetRef AssetManager::getAssetRef( const fs::path &path )
{
//	lock_guard<mutex> lock( mAssetsLock );

	uint32_t hash = makeUuid( path.generic_string() );
	auto asset = mAssets[hash];

	if( ! asset ) {
		asset = Asset::create( path, hash );
		mAssets[hash] = asset;

		// Add hash only after adding the Asset, to make sure the thread won't go looking for a missing asset.
		mAssetIds.push_back( hash );
	}

	// Update file modification date.
	asset->isModified();

	return asset;
}

AssetGroupRef AssetManager::getAssetGroupRef( uint32_t hash )
{
	auto group = mGroups[hash];

	if( ! group ) {
		group = AssetGroup::create( hash );
		mGroups[hash] = group;
	}

	return group;
}

ci::DataSourceRef AssetManager::findFile( const ci::fs::path &filePath )
{
#if defined( CINDER_ANDROID )
	// assets are handled specially for android, they need to be routed through AAssetManager
	return DataSourceAndroidAsset::create( filePath );
#elif defined( MASON_DEPLOY )
	// on other platforms, route through AssetArchiver for deploy mode
	if( ! mArchiver )
		throw AssetManagerExc( "no AssetArchiver present, needed for deploy mode." );

	return mArchiver->getAsset( filePath );
#else
	// check app assets folder
	auto fullPath = app::getAssetPath( filePath );
	if( ! fullPath.empty() && fs::exists( fullPath ) )
		return loadFile( fullPath );

	// only paths that are relative to assets folder are acceptable.
	if( fs::exists( filePath ) ) {
		throw AssetManagerExc( "file path was already resolved, yet it must be relative to assets folder (provided path: " + filePath.string() + ")." );
	}

	throw AssetManagerExc( "could not find asset with file path: " + filePath.string() );
#endif
}

void AssetManager::onFileChanged( const WatchEvent &event )
{
	// Flag groups as modified if asset was modified since the last check.
	// Groups remain modified until they are reloaded, so we only need to flag them once.
	auto asset = getAssetRef( event.getFile() );
	if( asset /* && asset->isModified() */ ) {
		// Assumes groups never delete themselves from the asset on the main thread.
		bool inUse = false;
		for( auto &ref : asset->mGroups ) {
			auto group = ref.lock();
			if( group ) {
				group->setModified( true );
				inUse = true;

				// Reset error state, so a new error can be shown.
				mAssetErrors.erase( group->getUuid() );
			}
		}
		asset->setInUse( inUse );

		// If this is a texture, force it to update.
		//if( inUse ) {
		//	auto texture = mTextures[asset->getUuid()].lock();
		//	if( texture ) {
		//		try {
		//			getTexture( path );
		//		}
		//		catch( const std::exception &exc ) {
		//			CI_LOG_EXCEPTION( "Failed to get texture", exc );
		//		}
		//	}
		//}
	}
}

void AssetManager::addShaderDefine( const std::string &define )
{
	mShaderPreprocessor->addDefine( define );
}

void AssetManager::addShaderDefine( const std::string &define, const std::string &value )
{
	mShaderPreprocessor->addDefine( define, value );
}

// ----------------------------------------------------------------------------------------------------
// Asset Archiving
// ----------------------------------------------------------------------------------------------------

void AssetManager::writeArchive( const ci::DataTargetRef &dataTarget )
{
//	mArchiver.reset( new AssetArchiver );
//
//	mArchiver->writeArchive( dataTarget );
}

void AssetManager::readArchive( const ci::DataSourceRef &dataSource )
{
//	mArchiver.reset( new AssetArchiver );
//
//	mArchiver->readArchive( dataSource );
}

// ----------------------------------------------------------------------------------------------------
// Asset
// ----------------------------------------------------------------------------------------------------

#if defined( CINDER_UWP ) || ( defined( _MSC_VER ) && ( _MSC_VER >= 1900 ) )
#define ASSET_INITIAL_TIME_MODIFIED std::chrono::system_clock::now()
#else
#define ASSET_INITIAL_TIME_MODIFIED std::time_t( 0 )
#endif

Asset::Asset( const fs::path& path, uint32_t uuid )
	: mPath( path ), mUuid( uuid ), mInUse( false ), mTimeModified( ASSET_INITIAL_TIME_MODIFIED )
{
	mConnection = FileWatcher::instance().watch( path, FileWatcher::Options().callOnWatch( false ), bind( &AssetManager::onFileChanged, AssetManager::instance(), placeholders::_1 ) );
}

Asset::~Asset()
{
}

bool Asset::isModified() const
{
	if( fs::exists( mPath ) ) {
		auto modified = fs::last_write_time( mPath );
		if( modified != mTimeModified ) {
			mTimeModified = modified;
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------------------------------------------
// AssetGroup
// ----------------------------------------------------------------------------------------------------

void AssetGroup::addAsset( const AssetRef &asset )
{
	if( find( mAssets.begin(), mAssets.end(), asset ) == mAssets.end() ) {
		mAssets.push_back( asset );
		asset->addGroup( shared_from_this() );
	}
}

void AssetGroup::setModified( bool modified )
{
	mIsModified = modified;

	if( mIsModified )
		mSignalModified.emit();
}

ci::signals::Connection AssetGroup::addModifiedCallback( const std::function<void()> &callback )
{
	return mSignalModified.connect( callback );
}

} // namespace mason
