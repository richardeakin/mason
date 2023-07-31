/*
Copyright 2021-2023 Richard Eakin - MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the “Software”), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "mason/Info.h"

#include "cinder/gl/Texture.h"

namespace mason {

//! Structure to store dense volume data (eg. SDF, density, color) contained within an eulerian grid.
//! - Provides a Grid3D for cpu storage axess, or ci::gl::Texture3dRef for gpu storage
//! - Uses Cinder's Image I/O API for loading to and from file. Supports EXR files. TODO: support png or other formats, converet to float for cpu storage
class Volume {
public:
	Volume();

	//! Stores the voxels that we are volumizing into
	struct Grid3D {
		using Voxel = glm::vec4;
		using ivec3 = glm::ivec3;

		Grid3D( const ivec3 &dims = ivec3( 16 ) );

		void resize( const ivec3 &dims );

		void set( const ivec3 &loc, const Voxel &v, int index = 0 );
		const Voxel&	get( const ivec3 &loc, int index = 0 ) const;

		const ivec3&	getDimensions() const	{ return mDims; }
		size_t			getSize() const			{ return mData.size(); }
		size_t			getNumVoxels() const	{ return mDims.x * mDims.y * mDims.z; }
		const Voxel*	getData() const			{ return mData.data(); }
		Voxel*			getData()				{ return mData.data(); }

	private:
		ivec3				mDims;
		std::vector<Voxel>	mData;
		std::vector<Voxel>	mData2; // temp to store 2nd EXR. TODO: just use mData, internal functions will figure out how to stripe data together in vec4s

		friend class Volume;
	};

	ci::vec3	getPos() const					{ return mPosition; }
	ci::ivec3	getDimensions() const			{ return mDimensions; }
	float		getScale()						{ return mScale; }
	const std::string& getDebugLabel() const	{ return mDebugLabel; }
	bool		getUseHalfFloat() const			{ return mUseHalfFloat; }

	ci::gl::Texture3dRef getTexture( int index = 0 ) const;

	//! Loads settings from info
	void load( const ma::Info &info );
	//! Saves settings to info
	void save( ma::Info &info )	const;

	//! Returns true if the volume should be recomputed
	bool updateUI();

	//! Imports a Volume given a volume dir (contains SDFa.exr, SDFb.exr, and settings.json for how to interpret those)
	//! - sets read-only to false
	void import( const ci::fs::path &volumeDir );
	//! Exports a Volume given a volume dir (contains SDFa.exr, SDFb.exr, and settings.json for how to interpret those)
	void exportEXR( const ci::fs::path &volumeDir );

	//! initializes empty buffers from current settings. Sets read-only to false
	void init();
	void clearTextures();

	const Volume::Grid3D&	getGrid() const { return mGrid; }
	Volume::Grid3D&			getGrid()		{ return mGrid; }

	// debug ---

	//! Returns the Volume::Grid3D (index = 0) after uploading to the gpu.
	ci::gl::Texture3dRef debugGetGridAsTexture() const;

private:

	ci::gl::Texture3dRef importVolumeFromEXR( const ci::fs::path &fullPath, int index );
	void exportVolumeToEXR( const ci::gl::Texture3dRef &tex, const ci::fs::path &outputFullPath, int numChannels, int textureIndex );
	void loadCPU( const ci::Surface32f &surface, int tilesPerRow, int tileSize, int index );
	ci::gl::Texture3dRef loadGPU( const ci::Surface32f &surface, int tilesPerRow, int tileSize );

	ci::ivec3	mDimensions = { 64, 64, 64 };
	ci::vec3	mPosition = { 0, 0, 0 };
	float		mScale =	1.0f;
	bool		mUseHalfFloat = true;
	bool		mUIEnabled = true;
	bool		mReadOnly = false;
	std::string	mDebugLabel;
	bool		mVolume0Enabled = true;
	bool		mVolume1Enabled = false;

	Grid3D					mGrid; // used for cpu-side storage
	ci::gl::Texture3dRef	mTexture0, mTexture1; // used for gpu-side storage

	// data not needed for import / export usage (gui only etc)
	bool		mForceUniformDimensions = true;
	bool		mNeedsInit = true;
};


namespace VolumeMath {

using ci::ivec3;
using ci::vec3;

ivec3	arrayIndexToVec3( int i, const ivec3 &dim );
size_t	vec3ToArrayIndex( const ivec3 loc, const ivec3 &dims );
vec3	scaleCellToVolume( ivec3 cell, ivec3 dim, vec3 min, vec3 max );
ivec3	scaleVolumeToCell( vec3 volumePos, ivec3 dim, vec3 min, vec3 max );

} // VolumeMath

} // namespace mason
