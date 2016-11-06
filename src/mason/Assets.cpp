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
	mShaderPreprocessor.addSearchDirectory( mason::getGlslDirectory() );

#if defined( CINDER_GL_ES )
	mShaderPreprocessor.setVersion( 300 );
	mShaderPreprocessor.addDefine( "CINDER_GL_ES" );
#else
	mShaderPreprocessor.setVersion( 410 );
	mShaderPreprocessor.addDefine( "CINDER_GL_CORE" );
#endif
}

AssetManager::~AssetManager()
{
}

// static
uint32_t AssetManager::uuid( const string &str )
{
	hash<string> hasher;
	return uint32_t( hasher( str ) );
}

//! Returns the requested shader. Loads the files synchronously if the shader is not cached. Returns empty shader if the shader could not be compiled.
gl::GlslProgRef AssetManager::getShader( const fs::path &vertex, const gl::GlslProg::Format &format )
{
	uint32_t hash = uuid( vertex.generic_string() );

	gl::GlslProgRef shader = mShaders[hash].lock();
	if( ! shader || mGroups[hash]->isModified() ) {
		auto vertFile = findFile( vertex );

		try {
			auto formatCopy = format;
			formatCopy.vertex( vertFile );

			auto group = getAssetGroupRef( hash );
			shader = reloadShader( formatCopy, group, hash );
			mShaders[hash] = shader;

			notifyResourceReloaded();
		}
		catch( const exception &exc ) {
			if( mAssetErrors.count( hash ) < 1 ) {
				mAssetErrors[hash] = true;
				CI_LOG_EXCEPTION( "Failed to compile glsl: " << vertex.filename(), exc );
			}
		}
	}

	return shader;
}

//! Returns the requested shader. Loads the files synchronously if the shader is not cached. Returns empty shader if the shader could not be compiled.
gl::GlslProgRef AssetManager::getShader( const fs::path& vertex, const fs::path& fragment, const gl::GlslProg::Format &format )
{
	uint32_t hash = uuid( vertex.generic_string() + fragment.generic_string() );

	gl::GlslProgRef shader = mShaders[hash].lock();
	if( ! shader || mGroups[hash]->isModified() ) {
		auto vert = findFile( vertex );
		auto frag = findFile( fragment );

		try {
			auto formatCopy = format;
			formatCopy.vertex( vert );
			formatCopy.fragment( frag );

			auto group = getAssetGroupRef( hash );
			shader = reloadShader( formatCopy, group, hash );

			notifyResourceReloaded();
		}
		catch( const exception &exc ) {
			if( mAssetErrors.count( hash ) < 1 ) {
				mAssetErrors[hash] = true;
				CI_LOG_EXCEPTION( "Failed to compile glsl: [" << vertex.filename() << "," << fragment.filename() << "]", exc );
			}
		}
	}

	return shader;
}

ci::signals::Connection AssetManager::getShader( const fs::path &vertex, const gl::GlslProg::Format &format, const function<void( gl::GlslProgRef )> &updateCallback )
{
	uint32_t hash = uuid( vertex.generic_string() );

	auto glslModifiedCallback = [this, format, hash, updateCallback, vertex] {
		try {
			// Update the format's source.
			DataSourceRef vertFile = findFile( vertex );

			// need to make an additional copy of format because the one captured by value in the lambda is const
			auto formatCopy = format;
			formatCopy.vertex( vertFile );

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

ci::signals::Connection AssetManager::getShader( const fs::path &vertex, const fs::path &fragment, const gl::GlslProg::Format &format, const function<void( gl::GlslProgRef )> &updateCallback )
{
	uint32_t hash = uuid( vertex.generic_string() + fragment.generic_string() );

	if( vertex.filename() == "scene.vert" ) {
		int blarg = 2;
	}

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

ci::gl::GlslProgRef AssetManager::reloadShader( ci::gl::GlslProg::Format &format, const AssetGroupRef &group, uint32_t hash )
{
	format.setPreprocessingEnabled( false ); // we use our own, preconfigured ShaderPreprocessor

	std::vector<std::pair<ci::fs::path, std::string>>	sources;

	// copy shader paths out first, resetting source on GlslProg::Format will clear them
	auto vertPath = format.getVertexPath();
	auto fragPath = format.getFragmentPath();

	// always add the asset paths and clear modified indicator, which enables the shader to be reloaded even if it errored on initial load
	group->addAsset( getAssetRef( vertPath ) );
	group->setModified( false );

	std::vector<fs::path>					includedFiles;

	std::set<fs::path> stageIncludedFiles;
	string parsedVertex = mShaderPreprocessor.parse( format.getVertex(), vertPath, &stageIncludedFiles );
	format.vertex( parsedVertex );
	sources.push_back( { vertPath, parsedVertex } );
	includedFiles.insert( includedFiles.end(), stageIncludedFiles.begin(), stageIncludedFiles.end() );

	if( ! fragPath.empty() ) {
		group->addAsset( getAssetRef( fragPath ) );

		stageIncludedFiles.clear();
		string parsedFrag = mShaderPreprocessor.parse( format.getFragment(), fragPath, &stageIncludedFiles );
		format.fragment( parsedFrag );
		sources.push_back( { fragPath, parsedFrag } );
		includedFiles.insert( includedFiles.end(), stageIncludedFiles.begin(), stageIncludedFiles.end() );
	}

	// all included files before trying to create the shader, so that in the case of an error we are still watching those files too.
	for( const auto &includeFile : includedFiles ) {
		group->addAsset( getAssetRef( includeFile ) );
	}

	auto shader = gl::GlslProg::create( format );
	mShaders[hash] = shader;


	mAssetErrors.erase( hash );

	mSignalShaderLoaded.emit( shader, sources );

	return shader;
}

//! Returns the requested texture. Loads the file synchronously if the texture is not cached. Returns empty texture if the file failed to load.
gl::Texture2dRef AssetManager::getTexture( const fs::path& path )
{
	auto dataSource = findFile( path );
	fs::path fullPath = dataSource->getFilePathHint(); // works with both file and archived assets
	uint32_t hash = uuid( fullPath.generic_string() );

	gl::Texture2dRef texture = mTextures[hash].lock();
	if( ! texture || mGroups[hash]->isModified() ) {

		try {
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

			auto group = getAssetGroupRef( hash );
			group->addAsset( getAssetRef( fullPath ) );
			group->setModified( false );

			mAssetErrors.erase( hash );

			notifyResourceReloaded();
		}
		catch( const exception &exc ) {
			if( mAssetErrors.count( hash ) < 1 ) {
				mAssetErrors[hash] = true;
				CI_LOG_EXCEPTION( "Failed to load texture: " << path.filename(), exc );
			}
		}
	}
	
	return texture;
}

/*
signals::Connection AssetManager::getFile( const fs::path &path, const std::function<void( DataSourceRef )> &updateCallback )
{
	uint32_t hash = uuid( path.generic_string() );
	auto assetIt = mAssets.find( hash );
	if( assetIt != mAssets.end() && ! assetIt->second->isModified() ) {
		// TODO: this should return a DataSourceRef stored on Asset, which is possibly our custom one that looks up in bin stream.
		updateCallback( loadFile( assetIt->second->getPath() ) );

		// TODO: how to return connection here?
		return ci::signals::Connection();
	}
	else {
		auto assetFile = findFile( path );
		auto group = getAssetGroupRef( hash );
		group->addAsset( getAssetRef( path ) );
	}

	return ci::signals::Connection();
}
*/

mason::WatchRef AssetManager::getFile( const fs::path &path, const std::function<void( DataSourceRef )> &updateCallback )
{
	try {
#if defined( OOBE_DEPLOY ) || defined( CINDER_ANDROID )
		auto assetFile = findFile( path );
		updateCallback( assetFile );
		return mason::WatchRef();
#else
		auto watch = mason::FileWatcher::load( path, [updateCallback]( const ci::fs::path &fullPath ) {
			updateCallback( loadFile( fullPath ) );
		} );

		return watch;
#endif
	}
	catch( std::exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load file for path: " << path, exc );
		CI_LOG_E( "stacktrace:\n" << ma::stackTraceAsString( 0, 4 ) );
		return mason::WatchRef();
	}
}

ci::DataSourceRef AssetManager::loadAsset( const ci::fs::path &path )
{
	return findFile( path );
}

void AssetManager::enableLiveAssets( bool enabled )
{
	ma::FileWatcher::setWatchingEnabled( enabled );
}

bool AssetManager::isLiveAssetsEnabled() const
{
	return true;
}

void AssetManager::cleanup()
{
	// Gather stats.
	size_t numGroups = mGroups.size();
	size_t numShaders = mShaders.size();
	size_t numTextures = mTextures.size();
	size_t numAssets = mAssets.size();

	// Remove empty weak pointers to expired shaders. Also remove corresponding groups.
	for( auto itr = mShaders.begin(); itr != mShaders.end(); ) {
		auto shader = itr->second.lock();
		if( ! shader ) {
			mGroups.erase( itr->first );
			itr = mShaders.erase( itr );
		}
		else
			++itr;
	}

	// Remove empty weak pointers to expired textures. Also remove corresponding groups.
	for( auto itr = mTextures.begin(); itr != mTextures.end(); ) {
		auto texture = itr->second.lock();
		if( ! texture ) {
			mGroups.erase( itr->first );
			itr = mTextures.erase( itr );
		}
		else
			++itr;
	}

	// Remove all assets that do not belong to a group. We can't rely on live assets being active, so update use state here.
//	mAssetsLock.lock();

	for( auto itr = mAssets.begin(); itr != mAssets.end(); ) {
		auto asset = itr->second;

		if( asset->isInUse() ) {
			asset->setInUse( false );
			for( auto &gref : asset->mGroups ) {
				auto group = gref.lock();
				if( group ) {
					asset->setInUse( true );
					break;
				}
			}
		}

		if( ! asset->isInUse() ) {
			// Remove both the asset and its id.
			mAssetIds.erase( remove( mAssetIds.begin(), mAssetIds.end(), itr->first ), mAssetIds.end() );
			itr = mAssets.erase( itr );
		}
		else
			++itr;
	}

//	mAssetsLock.unlock();

	// Log stats.
	numGroups -= mGroups.size();
	numShaders -= mShaders.size();
	numTextures -= mTextures.size();
	numAssets -= mAssets.size();

	CI_LOG_I( "Cleaned up assets: removed " << numGroups << " assets (" << numTextures << " textures, " << numShaders << " shaders) and " << numAssets << " files." );
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

	uint32_t hash = uuid( path.generic_string() );
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
#elif defined( OOBE_DEPLOY )
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

void AssetManager::onFileChanged( const fs::path &path )
{
	// Flag groups as modified if asset was modified since the last check.
	// Groups remain modified until they are reloaded, so we only need to flag them once.
	auto asset = getAssetRef( path );
	if( asset /* && asset->isModified() */ ) {
		// Assumes groups never delete themselves from the asset on the main thread.
		bool bInUse = false;
		for( auto &ref : asset->mGroups ) {
			auto group = ref.lock();
			if( group ) {
				group->setModified( true );
				bInUse = true;

				// Reset error state, so a new error can be shown.
				mAssetErrors.erase( group->getUuid() );
			}
		}
		asset->setInUse( bInUse );

		// If this is a texture, force it to update.
		if( bInUse ) {
			auto texture = mTextures[asset->getUuid()].lock();
			if( texture ) {
				try {
					getTexture( path );
				}
				catch( const std::exception &exc ) {
					CI_LOG_EXCEPTION( "Failed to get texture", exc );
				}
			}
		}
	}
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

Asset::Asset( const fs::path& path, uint32_t uuid )
	: mPath( path ), mUuid( uuid ), mInUse( false ), mTimeModified( 0 )
{
	mWatch = mason::FileWatcher::watch( path, bind( &AssetManager::onFileChanged, AssetManager::instance(), placeholders::_1 ) );
}

Asset::~Asset()
{
	if( mWatch )
		mWatch->unwatch();
}

bool Asset::isModified() const
{
	if( fs::exists( mPath ) ) {
		time_t modified = fs::last_write_time( mPath );
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