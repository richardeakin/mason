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

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Exception.h"
#include "cinder/Signals.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/ShaderPreprocessor.h" // TODO: forward declare

#include "mason/Mason.h"
//#include "mason/AssetArchiver.h"
#include "mason/FileWatcher.h"

#include <map>

//! If \c true, attempts to replace image data on reload without modifying the texture.
// TODO: remove or make option
#define USE_DEEP_LOADING		1

namespace mason {

class Asset;
class AssetGroup;
class AssetManager;

class IAsset {
public:
	virtual ~IAsset() {}

	//! Returns the unique identifier for this asset.
	virtual uint32_t uuid() const = 0;
	//! Adds a path to the asset's file list.
	virtual IAsset& add( const ci::fs::path &path ) = 0;
	//! Loads the asset(s) synchronously and returns TRUE if successful.
	virtual bool load() = 0;
	//! Returns a string describing the last error.
	virtual const std::string& what() const = 0;
};

//----------------------------------------------------------------------------------------------------

typedef std::shared_ptr<Asset> AssetRef;

class Asset {
	friend AssetGroup;
	friend AssetManager;

public:
	Asset( const ci::fs::path& path, uint32_t uuid );
	~Asset();

	static std::shared_ptr<Asset> create( const ci::fs::path& path, uint32_t uuid ) { return std::make_shared<Asset>( path, uuid ); }

	const ci::fs::path& getPath() const { return mPath; }
	uint32_t getUuid() const { return mUuid; }

private:
	Asset() : Asset( ci::fs::path(), 0 ) {}

	bool isModified() const;

	bool isInUse() const { return mInUse; }
	void setInUse( bool inUse ) { mInUse = inUse; }

	void addGroup( const std::shared_ptr<AssetGroup> &group )
	{
		mGroups.push_back( group );
		mInUse = true;
	}

private:
	const ci::fs::path  mPath;
	const uint32_t      mUuid;

	bool                mInUse;

	mutable ci::fs::file_time_type	mTimeModified;

	std::vector<std::weak_ptr<AssetGroup>>  mGroups;

	ci::signals::ScopedConnection mConnection;
};

//----------------------------------------------------------------------------------------------------

typedef std::shared_ptr<AssetGroup> AssetGroupRef;
typedef std::weak_ptr<AssetGroup> AssetGroupWeakRef;

class AssetGroup : public std::enable_shared_from_this < AssetGroup > {
	friend AssetManager;

public:
	AssetGroup( uint32_t uuid ) : mUuid( uuid ), mIsModified( false ) {}
	~AssetGroup() {}

	static std::shared_ptr<AssetGroup> create( uint32_t uuid ) { return std::make_shared<AssetGroup>( uuid ); }

	uint32_t getUuid() const { return mUuid; }

private:
	AssetGroup() : AssetGroup( 0 ) {}

	bool isModified() const { return mIsModified; }
	void setModified( bool modified = true );
	void addAsset( const AssetRef &asset );
	ci::signals::Connection addModifiedCallback( const std::function<void ()> &callback );

	const uint32_t         mUuid;
	bool                   mIsModified;
	std::vector<AssetRef>  mAssets;
	ci::signals::Signal<void ()>	mSignalModified;
};

//----------------------------------------------------------------------------------------------------

typedef ci::signals::Signal<void( const ci::gl::GlslProgRef &, const std::vector<std::pair<ci::fs::path, std::string>> & )>	SignalShaderLoaded;

class MA_API AssetManager {
public:
	static const int kSeed = 9213;

public:
	static AssetManager* instance();
	~AssetManager();

	//! Returns the requested shader within the provided \a updateCallback upon initial load and any time it is updated on file.
	ci::signals::Connection getShader( const ci::fs::path& vertex, const ci::gl::GlslProg::Format &format, const std::function<void( ci::gl::GlslProgRef )> &updateCallback );
	//! Returns the requested shader within the provided \a updateCallback upon initial load and any time it is updated on file.
	ci::signals::Connection getShader( const ci::fs::path& vertex, const ci::fs::path& fragment, const ci::gl::GlslProg::Format &format, const std::function<void( ci::gl::GlslProgRef )> &updateCallback );
	//! Returns the requested shader within the provided \a updateCallback upon initial load and any time it is updated on file.
	ci::signals::Connection getShader( const ci::fs::path& vertex, const std::function<void( ci::gl::GlslProgRef )> &updateCallback )	{ return getShader( vertex, ci::gl::GlslProg::Format(), updateCallback ); }
	//! Returns the requested shader within the provided \a updateCallback upon initial load and any time it is updated on file.
	ci::signals::Connection getShader( const ci::fs::path& vertex, const ci::fs::path& fragment, const std::function<void( ci::gl::GlslProgRef )> &updateCallback ) { return getShader( vertex, fragment, ci::gl::GlslProg::Format(), updateCallback ); }

	//! Returns the requested texture within the provided \a updateCallback upon initial load and any time it is updated on file. Loads synchronously if the texture is not cached.
	ci::signals::Connection getTexture( const ci::fs::path &texturePath, const std::function<void( ci::gl::Texture2dRef )> &updateCallback  );
	//! Calls \a updateCallback whenever the file at \a path is modified and needs to be relaoded. Returns a WatchRef to handle the scope of the associated file watch (empty in deploy mode)
	ci::signals::Connection getFile( const ci::fs::path &path, const std::function<void( ci::DataSourceRef )> &updateCallback );
	//! Loads and returns a DataSourceRef for the file associated with \a path.
	ci::DataSourceRef loadAsset( const ci::fs::path &path );

	//! Enables or disables live assets. Files are checked for modifications at regular intervals and automatically reloaded if they have changed.
	void enableLiveAssets( bool enabled = true );
	//! Disables asset modification checks.
	void disableLiveAssets() { enableLiveAssets( false ); }
	//! Returns TRUE if asset modification checks are enabled.
	bool isLiveAssetsEnabled() const;

	// Returns a signal that is emitted whenever a shader is parsed. Useful for adding UI based on that shader's params. args: 1) shader path, 2) shader source.
	SignalShaderLoaded&	getSignalShaderLoaded()		{ return mSignalShaderLoaded; }

	//! Removes weak references to assets that are no longer in use. Not required for normal operation. This is a potentially slow operation, use with care.
	void cleanup();

	//! Returns the total number of different shaders loaded, including expired ones. Call cleanup() first if you want to exclude expired shaders.
	size_t getShaderCount() const { return mShaders.size(); }

	//! Returns the total number of different textures loaded, including expired ones. Call cleanup() first if you want to exclude expired textures.
	size_t getTextureCount() const { return mTextures.size(); }

	//! Returns the total number of different assets loaded, including expired ones. Call cleanup() first if you want to exclude expired assets.
	size_t getAssetCount() const { return mGroups.size(); }

	//! Returns the total number of different files loaded, including expired ones. Call cleanup() first if you want to exclude expired files.
	size_t getFileCount() const { return mAssets.size(); }

	//! Adds the paths of all files currently in use to \a paths.
	void getFilesInUse( std::vector<ci::fs::path> *paths ) const;

	//! Constructs the assets binary archive for deployment
	void writeArchive( const ci::DataTargetRef &dataTarget );
	//!
	void readArchive( const ci::DataSourceRef &dataSource );

	ci::gl::ShaderPreprocessor*	getShaderPreprocessor()	{ initShaderPreprocessorLazy(); return mShaderPreprocessor.get(); }

private:
	AssetManager();

	void				initShaderPreprocessorLazy();
	//! \note: will modify format
	ci::gl::GlslProgRef reloadShader( ci::gl::GlslProg::Format &format, const AssetGroupRef &group, uint32_t hash );


	AssetRef			getAssetRef( const ci::fs::path &path );
	AssetGroupRef		getAssetGroupRef( uint32_t hash );
	ci::DataSourceRef	findFile( const ci::fs::path &filePath );

	friend class Asset;

	void onFileChanged(  const WatchEvent &event );

	std::map<uint32_t, std::weak_ptr<ci::gl::GlslProg>>   mShaders;
	std::map<uint32_t, std::weak_ptr<ci::gl::Texture2d>>  mTextures;

	std::unique_ptr<ci::gl::ShaderPreprocessor>			mShaderPreprocessor;
	SignalShaderLoaded									mSignalShaderLoaded;

	std::map<uint32_t, AssetGroupRef>    mGroups;
	std::map<uint32_t, AssetRef>         mAssets;
	std::vector<uint32_t>                mAssetIds;

	std::map<uint32_t, bool>             mAssetErrors;

	ci::signals::Connection              mConnection;

	//std::unique_ptr<AssetArchiver>		mArchiver;
};

static inline AssetManager* assets() { return AssetManager::instance(); }

class AssetManagerExc : public ci::Exception {
  public:
	AssetManagerExc( const std::string &description )
		: Exception( description )
	{}
};

} // namespace mason
