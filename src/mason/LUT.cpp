/*
Copyright (c) 2017, Richard Eakin - All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided
that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
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

#include "mason/LUT.h"
#include "mason/Export.h"

using namespace ci;
using namespace std;


namespace mason {

template <typename ColorT>
ColorLUT<ColorT>::ColorLUT( size_t size, const std::vector<Stop> &stops )
	: mLUT( size ), mStops( stops )
{
	sortStops();
	fillTable();
}

template <typename ColorT>
ColorLUT<ColorT>::ColorLUT( const ci::ImageSourceRef &imageSource )
{
	ci::Surface32f surface( imageSource );
	assert( surface.getHeight() == 1 );
	mLUT.resize( surface.getWidth() );

	for( size_t i = 0; i < surface.getWidth(); i++ )
		mLUT[i] = Colorf( surface.getPixel( ivec2( i, 0 ) ) );
}

template <typename ColorT>
const ColorT& ColorLUT<ColorT>::lookup( float f )
{
	size_t i = glm::clamp<size_t>( (size_t)round( f * (float)mLUT.size() ), 0, mLUT.size() - 1 );
	return mLUT[i];
}

template <typename ColorT>
Surface32f ColorLUT<ColorT>::makeSurface32()
{
	auto surface = Surface32f( mLUT.size(), 1, false );
	for( size_t i = 0; i < mLUT.size(); i++ ) {
		const auto &lut = mLUT[i];
		auto pixel = Colorf( CHANTRAIT<float>::convert( lut.r ), CHANTRAIT<float>::convert( lut.g ), CHANTRAIT<float>::convert( lut.b ) );
		surface.setPixel( ivec2( i, 0 ), pixel );
	}

	return surface;
}

template <typename ColorT>
Surface32f ColorLUT<ColorT>::makeSurface32a()
{
	auto surface = Surface32f( mLUT.size(), 1, true );
	for( size_t i = 0; i < mLUT.size(); i++ ) {
		const auto &lut = mLUT[i];
		//auto pixel = ColorAf( CHANTRAIT<float>::convert( lut.r ), CHANTRAIT<float>::convert( lut.g ), CHANTRAIT<float>::convert( lut.b ) );
		auto pixel = ColorAf( lut );
		surface.setPixel( ivec2( i, 0 ), pixel );
	}

	return surface;
}

template <typename ColorT>
void ColorLUT<ColorT>::sortStops()
{
	stable_sort( mStops.begin(), mStops.end(),
		[]( const Stop &a, const Stop &b ) { return a.percent < b.percent; }
	);
}

template <typename ColorT>
void ColorLUT<ColorT>::fillTable()
{
	if( mStops.size() < 2 ) {
		// fill with only first stop if size = 1, or black if 0
		ColorT col = mStops.size() == 1 ? mStops[0].color : Colorf::black();
		std::fill( mLUT.begin(), mLUT.end(), col );
		return;
	}

	for( size_t s = 0; s < mStops.size() - 1; s++ ) {
		const auto &stop0 = mStops[s];
		const auto &stop1 = mStops[s + 1];

		size_t startIndex = glm::clamp<size_t>( round( stop0.percent * mLUT.size() ), 0, mLUT.size() - 1 );
		size_t stopIndex = glm::clamp<size_t>( round( stop1.percent * mLUT.size() ), 0, mLUT.size() - 1 );


		size_t numBins = stopIndex - startIndex;
		if( numBins < 1 )
			continue;

		for( size_t i = 0; i < numBins; i++ ) {
			float t = float( i ) / float( numBins );
			mLUT[startIndex + i] = lerp( stop0.color, stop1.color, t );
		}

		// If last stop and we're not at the end, fill with remainder
		if( ( s + 1 == mStops.size() - 1 ) ) {
			for( size_t i = stopIndex; i < mLUT.size(); i++ ) {
				mLUT[i] = stop1.color;
			}
		}
	}
}

template class MA_API ColorLUT<Color8u>;
template class MA_API ColorLUT<Colorf>;
template class MA_API ColorLUT<ColorA8u>;
template class MA_API ColorLUT<ColorAf>;

} // namespace mason
