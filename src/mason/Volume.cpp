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

#include "mason/Volume.h"

#include "cinder/Log.h"
#include "cinder/app/App.h"
#include "cinder/gl/scoped.h"

#include "jsoncpp/json.h"
#include "mason/imx/ImGuiStuff.h"
#include "mason/imx/ImGuiTexture.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason {

// ----------------------------------------------------------------------------------
// VolumeMath
// ----------------------------------------------------------------------------------

namespace VolumeMath {

ivec3 arrayIndexToVec3( int i, const ivec3 &dim )
{
	ivec3 result;
	result.x = i % dim.x;
	result.y = ( i / dim.x ) % dim.y;
	result.z = i / ( dim.x * dim.y );
	return result;
}

size_t vec3ToArrayIndex( const ivec3 loc, const ivec3 &dims )
{
	return (size_t)( (loc.z * dims.x * dims.y) + (loc.y * dims.x) + loc.x );
}
vec3 scaleCellToVolume( ivec3 cell, ivec3 dim, vec3 min, vec3 max )
{
	return min + vec3( max - min ) * vec3( cell ) / vec3( dim - ivec3( 1 ) );	
}

ivec3 scaleVolumeToCell( vec3 volumePos, ivec3 dim, vec3 min, vec3 max )
{
	vec3 scaled = ( vec3( dim ) * ( volumePos - min ) ) / ( max - min );
	ivec3 loc = ivec3( scaled );
	CI_ASSERT( loc.x < dim.x && loc.y < dim.y && loc.z < dim.z );

	return loc;
}

} // VolumeMath

// ----------------------------------------------------------------------------------
// Grid3D
// ----------------------------------------------------------------------------------
// TODO: combine mData and mData2, striped

Volume::Grid3D::Grid3D( const glm::ivec3 &dimensions )
{
	resize( dimensions );
}

void Volume::Grid3D::resize( const glm::ivec3 &dimensions )
{
	mDims = dimensions;
	mData.resize( dimensions.x * dimensions.y * dimensions.z );
	mData2.resize( dimensions.x * dimensions.y * dimensions.z );
}

void Volume::Grid3D::set( const ivec3 &loc, const Voxel &v, int index )
{
	size_t coord = (size_t)( (loc.z * mDims.x * mDims.y) + (loc.y * mDims.x) + loc.x );
	CI_ASSERT( coord < mData.size() );

	if( index == 0 )
		mData[coord] = v;
	else
		mData2[coord] = v;
}

const Volume::Grid3D::Voxel& Volume::Grid3D::get( const ivec3 &loc, int index ) const
{
	size_t coord = (size_t)( (loc.z * mDims.x * mDims.y) + (loc.y * mDims.x) + loc.x );
	CI_ASSERT( coord < mData.size() );

	return index == 0 ? mData[coord] : mData2[coord];
}

// ----------------------------------------------------------------------------------
// Volume
// ----------------------------------------------------------------------------------

#define DEBUG_TEX_READ_WRITE 1

namespace {

gl::Texture2dRef sSurfaceTex, sSurfaceEXRImport, sSurfaceEXRExport;
int	sDebugInt = 0;

// TODO: remove this and use method to get the format
auto sDebugFormat2d = gl::Texture2d::Format()
	//.internalFormat( GL_RGBA32F )
	.internalFormat( GL_RGBA16F )
	//.wrap( GL_CLAMP_TO_EDGE )
	//.minFilter( GL_NEAREST ).magFilter( GL_NEAREST )
	.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
	.label( "Debug Texture2d" )
;

} // anon namespace

Volume::Volume()
{
}

gl::Texture3dRef Volume::getTexture( int index ) const
{
	CI_ASSERT( index <= 1 );
	
	return index == 0 ? mTexture0 : mTexture1;
}

void Volume::load( const ma::Info &info )
{
	mDimensions = info.get( "dimensions", mDimensions );
	mScale = info.get( "scale", mScale );
	mUseHalfFloat = info.get( "halfFloat", mUseHalfFloat );
	mDebugLabel = info.get<string>( "debugLabel", "" );
	mVolume0Enabled = info.get( "volume0", mVolume0Enabled );
	mVolume1Enabled = info.get( "volume1", mVolume1Enabled );
}

void Volume::save( ma::Info &info )	const
{
	info["dimensions"] = vec3( mDimensions );
	info["scale"] = mScale;
	info["halfFloat"] = mUseHalfFloat;
	info["volume0"] = mVolume0Enabled;
	info["volume1"] = mVolume1Enabled;
	info["debugLabel"] = mDebugLabel;
}

// loads info.json from volumeDir folder, uses it to adjust settings and resize volume if needed
void Volume::import( const fs::path &volumeDir )
{
	mReadOnly = true;

	// TODO: update this to use a specific file for <= 4 channels, pattern for multiple
	// - check if we can use exr for N channels

	auto infoFile = volumeDir / "info.json";
	if( fs::exists( infoFile ) ) {
		try {
			auto info = ma::Info::convert<Json::Value>( loadFile( infoFile ) );
			load( info );
		}
		catch( std::exception &exc ) {
			CI_LOG_EXCEPTION( "failed to load info.json", exc );
		}
	}

	if( mVolume0Enabled ) {
		auto SDFa = volumeDir / "SDFa.exr";
		if( fs::exists( SDFa ) ) {
			mTexture0 = importVolumeFromEXR( SDFa, 0 );
		}
		else {
			CI_LOG_E( "expected SDFa.exr file at path: " << SDFa );
		}
	}
	if( mVolume1Enabled ) {
		auto SDFb = volumeDir / "SDFb.exr";
		if( fs::exists( SDFb ) ) {
			mTexture1 = importVolumeFromEXR( SDFb, 1 );
		}
		else {
			CI_LOG_E( "expected SDFb.exr file at path: " << SDFb );
		}
	}

	if( mDebugLabel.empty() ) {
		mDebugLabel = "Volume (" + volumeDir.parent_path().string() + ")";
	}
}

gl::Texture3dRef Volume::importVolumeFromEXR( const fs::path &fullPath, int index )
{
	gl::Texture3dRef texture3d;

	try {
		Timer timer( true );

		CI_LOG_I( "fullPath: " << fs::canonical( fullPath ) );

		auto dataSource = app::loadAsset( fullPath );
		auto imageSource = loadImage( dataSource );

		auto surface = Surface32f( imageSource );

		auto col = surface.getPixel( ivec2( 0, 0 ) );
		CI_LOG_I( "first pixel: " << col );

		// assume that the 2d texture holds a square images.
		const ivec2 imageSize = surface.getSize();
		const int numPixels = surface.getWidth() * surface.getHeight();
		int tileSize = (int)lround( powf( float( numPixels ), 1.0f / 3.0f ) );

		int tilesPerRow =  (int)lround(sqrtf( (float)( surface.getWidth() / tileSize ) * ( surface.getHeight() / tileSize ) ) );
		CI_LOG_I( "image size: " << surface.getSize() << ", tile size: " << tileSize << ", tiles per row: " << tilesPerRow );

		if( index == 0 ) {
			// this will load both
			loadCPU( surface, tilesPerRow, tileSize, index );
		}

		texture3d = loadGPU( surface, tilesPerRow, tileSize );

		CI_LOG_I( "loaded file " << fullPath << " in " << timer.getSeconds() << " seconds." );
	}
	catch( std::exception &exc ) {
		CI_LOG_EXCEPTION( "failed to read exr file named: " << fullPath.filename(), exc );
		return {};
	}

	return texture3d;
}

// note to self: index is temporary until mData vector supports N voxel components
void Volume::loadCPU( const ci::Surface32f &surface, int tilesPerRow, int tileSize, int index )
{
	const ivec2 imageSize = surface.getSize();

	mGrid.resize( mDimensions );

	Timer timer( true );
	vec2 sdExtents( 10e6, -10e6 ); // TODO: move this to a property on the volume

	for( size_t tile = 0; tile < tilesPerRow * tilesPerRow; tile++ ) {
		const size_t copyBytes = tileSize * 4 * sizeof( float );
		const size_t sourceX = tileSize * ( tile % tilesPerRow );
		const size_t sourceY = tileSize * ( tile / tilesPerRow );
		size_t sourceIndex = sourceY * imageSize.x + sourceX;
		size_t destStart = ( tileSize * tileSize ) * tile;
		for( size_t row = 0; row < tileSize; row++, sourceIndex += imageSize.x, destStart += tileSize ) {			
			for( size_t pixel = 0; pixel < tileSize; pixel++ ) {
				auto col = surface.getPixel( ivec2( sourceX + pixel, sourceY + row ) );
				auto loc = ivec3( pixel, row, tile );

				mGrid.set( loc, Grid3D::Voxel( col.r, col.g, col.b, col.a ), index );

				vec4 voxel = mGrid.get( loc );

				float SD = col.a;
				if( sdExtents.x > SD )
					sdExtents.x = SD;
				else if( sdExtents.y < SD )
					sdExtents.y = SD;

				float bogieCheck = 10000;
				if( sdExtents.x < -bogieCheck || sdExtents.y > bogieCheck ) {
					//CI_LOG_E( "SD oddly large at loc: " << loc << ", sdExtents: " << sdExtents );
					int breakHere = 1;
				}
			}
		}
	}

	CI_LOG_I( "loaded CPU Grid3D in " << timer.getSeconds() << " seconds. SDF extents:" << sdExtents );
}

// FIXME: this appears to have broken sometime long ago.. (data striping)
gl::Texture3dRef Volume::loadGPU( const ci::Surface32f &surface, int tilesPerRow, int tileSize )
{
	const ivec2 imageSize = surface.getSize();
	const int numPixels = surface.getWidth() * surface.getHeight();

	auto texFormat = gl::Texture3d::Format()
		.internalFormat( mUseHalfFloat ? GL_RGBA16F : GL_RGBA32F )
		//.wrap( GL_CLAMP_TO_EDGE )
		.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		//.minFilter( GL_NEAREST ).magFilter( GL_NEAREST )
		.label( "Import EXR Volume" )
	;

	//if( mUseHalfFloat ) {
	//	texFormat.setDataType( GL_HALF_FLOAT );
	//}
	//else {
	//	texFormat.setDataType( GL_FLOAT );
	//}

	// copy data from tiles to stacked slices, one tile row at a time
	const ColorA* surfaceData = (ColorA *)surface.getData();
	vector<ColorA> buffer2( numPixels );
	for( size_t tile = 0; tile < tilesPerRow * tilesPerRow; tile++ ) {
		//const size_t copyBytes = tileSize * 4 * sizeof( float );
		const size_t sourceX = tileSize * ( tile % tilesPerRow );
		const size_t sourceY = tileSize * ( tile / tilesPerRow );
		size_t sourceIndex = sourceY * imageSize.x + sourceX;
		size_t destStart = ( tileSize * tileSize ) * tile;
		for( size_t row = 0; row < tileSize; row++, sourceIndex += imageSize.x, destStart += tileSize ) {
			//memcpy( &buffer2[destStart], &surfaceData[sourceIndex], copyBytes ); //TODO: use this again once debugged, I think it is fine
			for( size_t pixel = 0; pixel < tileSize; pixel++ ) {
				ColorA col = surface.getPixel( ivec2( sourceX + pixel, sourceY + row ) );

				CI_ASSERT( ( destStart + pixel ) < numPixels );
				buffer2[destStart + pixel] = col;
			}
		}
	}

#if DEBUG_TEX_READ_WRITE
	sSurfaceTex = gl::Texture::create( buffer2.data(), GL_RGBA, imageSize.x, imageSize.y, sDebugFormat2d );
	sSurfaceEXRImport = gl::Texture::create( surface, sDebugFormat2d.loadTopDown( true ) ); // this works but there is an annoying warning logged
#endif

	if( tileSize * tileSize * tileSize == surface.getWidth() * surface.getHeight() ) {
		return gl::Texture3d::create( buffer2.data(), GL_RGBA, tileSize, tileSize, tileSize, texFormat );
	}
	else {
		CI_LOG_E( "tileSize doesn't match image dimensions" );
	}

	return {};
}

void Volume::exportEXR( const fs::path &volumeDir )
{
	fs::path outputFilename0 = "SDFa.exr";
	fs::path outputFilename1 = "SDFb.exr";

	fs::create_directories( volumeDir );

	outputFilename0 = volumeDir / outputFilename0.string();
	outputFilename1 = volumeDir / outputFilename1.string();

	if( mVolume0Enabled ) {
		exportVolumeToEXR( mTexture0, outputFilename0, 4, 0 );
	}
	if( mVolume1Enabled ) {
		//	int numChannels2 = mExportVolumeReduceChannels ? (int)volume2NumChannels( mVolumeColorMode ) : 4;
		exportVolumeToEXR( mTexture1, outputFilename1, 4, 1 );
	}

	// write volume settings to folder
	{
		ma::Info volInfo;
		save( volInfo );

		ofstream ostream( ( volumeDir / "info.json" ).string().c_str() );
		ostream << volInfo;
		ostream.close();
	}
}

void Volume::exportVolumeToEXR( const ci::gl::Texture3dRef &tex, const fs::path &outputFullPath, int numChannels, int textureIndex )
{
	if( ! tex ) {
		CI_LOG_E( "null tex" );
		return;
	}

	CI_LOG_I( "exporting volume to: " << outputFullPath );

	// copy data to vector<ColorA> and push that to debug tex 2d
	try {
		Timer timer( true );

		const size_t numPixels = tex->getWidth() * tex->getHeight() * tex->getDepth();
		vector<ColorA>	buffer( numPixels );

		ivec2 imageSize;
		int tileSize;
		int tilesPerRow;
		size_t numTiles;

		// 128^3 needs different image layout from 64^3 or 256^3
		// assuming that all 3 dims are the same (for now - can change if that assumption changes)
		// also assuming that dimensions are powers of 2
		int widthLog2 = (int)lround(log2(tex->getWidth()));
		CI_LOG_I("tex->getWidth() = " << tex->getWidth() << "  widthLog2 = " << widthLog2);
		if (widthLog2 % 2 == 0) {
			// Rich's code for even power-of-2
			int imageDim = (int)lround(sqrtf(float(numPixels)));
			imageSize = ivec2(imageDim, imageDim);
			tileSize = (int)lround(powf(float(numPixels), 1.0f / 3.0f));
			tilesPerRow = (int)lround(sqrtf((float)(imageSize.x / tileSize) * (imageSize.y / tileSize)));
			numTiles = tilesPerRow * tilesPerRow;
		}
		else {
			// odd power-of-2
			int imageDim = (int)lround(sqrtf(float(numPixels/2)));
			imageSize = ivec2(imageDim*2, imageDim);
			tileSize = tex->getWidth();
			tilesPerRow = imageSize.x / tileSize;
			numTiles = tilesPerRow * tilesPerRow / 2;
		}

		CI_LOG_I( "\t- imageSize: " << imageSize << ", tileSize: " << tileSize << ", tilesPerRow: " << tilesPerRow << ", numTiles: " << numTiles );

		gl::ScopedTextureBind texScope( tex->getTarget(), tex->getId() );
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );

		GLenum format = GL_RGBA;
		GLenum dataType = GL_FLOAT;
		glGetTexImage( tex->getTarget(), 0, format, dataType, buffer.data() );

		// copy data from stacked slices to tiles, one tile row at a time
		// Need to figure out how to export 3 channel EXR still (probably need to set the bool in Surface32f below to false)
		auto imageTargetOptions = ImageTarget::Options();
		if( numChannels == 2 ) {
			// two channel EXR will copy the first 2 channels out of the Surface
			imageTargetOptions.colorModel( ImageIo::ColorModel::CM_GRAY );
		}

		Surface32f surface( imageSize.x, imageSize.y, true );
		ColorA *buffer2 = (ColorA *)surface.getData();
		for( size_t tileIndex = 0; tileIndex < numTiles; tileIndex++ ) {
			const size_t copyBytes = tileSize * 4 * sizeof( float );
			const size_t destX = tileSize * ( tileIndex % tilesPerRow );
			const size_t destY = tileSize * ( tileIndex / tilesPerRow );
			size_t destIndex = destY * imageSize.x + destX;
			size_t sourceStart = ( tileSize * tileSize ) * tileIndex;
			for( size_t row = 0; row < tileSize; row++, destIndex += imageSize.x, sourceStart += tileSize ) {			
				memcpy( &buffer2[destIndex], &buffer[sourceStart], copyBytes );
			}
		}

#if DEBUG_TEX_READ_WRITE
		if( textureIndex == 0 ) {
			//sSurfaceEXRExport = gl::Texture::create( buffer2, GL_RGBA, imageSize.x, imageSize.y, sDebugFormat2d );
			sSurfaceEXRExport = gl::Texture::create( surface, sDebugFormat2d.loadTopDown( true ) ); // this works but there is an annoying warning logged
		}
#endif

		writeImage( outputFullPath, surface, imageTargetOptions );
		CI_LOG_I( "wrote to file " << outputFullPath << " in " << timer.getSeconds() << " seconds, dimensions: " << imageSize << ", channels: " << numChannels );
	}
	catch( std::exception &exc ) {
		CI_LOG_EXCEPTION( "Failed to write to file " << outputFullPath, exc );
	}
}

void Volume::init()
{
	mReadOnly = false;

	auto format = gl::Texture3d::Format()
		.internalFormat( mUseHalfFloat ? GL_RGBA16F : GL_RGBA32F )
		//.wrap( GL_CLAMP_TO_EDGE )
		.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		//.minFilter( GL_NEAREST ).magFilter( GL_NEAREST )
		.label( mDebugLabel )
	;

	mTexture0 = gl::Texture3d::create( mDimensions.x, mDimensions.y, mDimensions.z, format );
	mTexture1 = gl::Texture3d::create( mDimensions.x, mDimensions.y, mDimensions.z, format );

	// - will also have a good idea once exrs are exported
	size_t totalBytes = mDimensions.x * mDimensions.y * mDimensions.z * sizeof( float ) * 4;
	if( mUseHalfFloat ) {
		totalBytes /= 2;
	}

	CI_LOG_I( "volume texture size: " << float( totalBytes ) / ( 1024 * 1024 ) << " mb" );

	clearTextures();
	mNeedsInit = false;
}

void Volume::clearTextures()
{
	if( mTexture0 ) {
		vec4 color( 0, 0, 0, 0 );
		glClearTexImage( mTexture0->getId(), 0, GL_RGBA, GL_FLOAT, &color[0] );
	}

	if( mTexture1 ) {
		vec4 color( 0, 0, 0, 0 );
		glClearTexImage( mTexture1->getId(), 0, GL_RGBA, GL_FLOAT, &color[0] );
	}
}

bool Volume::updateUI()
{
	bool volumeNeedsUpdate = false; // tells caller that they should run their volume process

	if( mReadOnly ) {
		imx::BeginDisabled();
	}

	int pushedColors = 0;
	if( mNeedsInit && ! mReadOnly ) {
		im::PushStyleColor( ImGuiCol_Text, ColorA( 1, 0.5f, 0, 1 ) );
		pushedColors += 1;
	}
	if( im::Button( "initialize" ) ) {
		init();
		volumeNeedsUpdate = true;
	}
	im::SameLine();
	im::Checkbox("force uniform axes", &mForceUniformDimensions );

	if( im::DragInt3( "dimensions", &mDimensions.x, 1, 1, 4096 ) ) {
		mNeedsInit = true;
		if( mForceUniformDimensions ) {
			int x = glm::max( glm::max( mDimensions.r, mDimensions.g ), mDimensions.b );
			mDimensions = ivec3( x );
		}
	}

	if( im::Checkbox( "use half float", &mUseHalfFloat ) ) {
		volumeNeedsUpdate = true;
	}

	if( mReadOnly ) {
		imx::EndDisabled();
	}

	im::InputText( "debug label", &mDebugLabel );
	im::DragInt( "debug int", &sDebugInt, 1, 0, 64 * 64 );

#if DEBUG_TEX_READ_WRITE
	imx::Texture2d( "Debug sSurfaceTex", sSurfaceTex );
	imx::Texture2d( "Debug sSurfaceEXRImport", sSurfaceEXRImport );
	imx::Texture2d( "Debug sSurfaceEXRExport", sSurfaceEXRExport );
#endif

	if( pushedColors ) {
		im::PopStyleColor( pushedColors);
	}

	return volumeNeedsUpdate;
}

ci::gl::Texture3dRef Volume::debugGetGridAsTexture() const
{
	const auto &dims  = getDimensions();

	auto format = gl::Texture3d::Format()
		.internalFormat( GL_RGBA32F )
		//.wrap( GL_CLAMP_TO_EDGE )
		//.minFilter( GL_LINEAR ).magFilter( GL_LINEAR )
		.minFilter( GL_NEAREST ).magFilter( GL_NEAREST )
		.label( "Volume::Grid Debug" )
	;

	CI_LOG_I( "uploading mVolume's Grid data to GPU.." );
	return gl::Texture3d::create( (const void *)mGrid.getData(), GL_RGBA, dims.x, dims.y, dims.z, format );
}

} // namespace mason
