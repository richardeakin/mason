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

using namespace ci;
using namespace std;


namespace mason {

ColorLUT::ColorLUT( size_t size, const std::vector<Stop> &stops )
	: mLUT( size ), mStops( stops )
{
	sortStops();
	fillTable();
}

ColorLUT::ColorLUT( const ci::ImageSourceRef &imageSource )
{
	ci::Surface32f surface( imageSource );
	assert( surface.getHeight() == 1 );
	mLUT.resize( surface.getWidth() );

	for( size_t i = 0; i < surface.getWidth(); i++ )
		mLUT[i] = Colorf( surface.getPixel( ivec2( i, 0 ) ) );
}

const Colorf& ColorLUT::lookup( float f )
{
	size_t i = clamp<size_t>( (size_t)round( f * (float)mLUT.size() ), 0, mLUT.size() - 1 );
	return mLUT[i];
}

Surface32f ColorLUT::makeSurface32f()
{
	auto surface = Surface32f( mLUT.size(), 1, false );
	for( size_t i = 0; i < mLUT.size(); i++ ) {
		surface.setPixel( ivec2( i, 0 ), mLUT[i] );
	}

	return surface;
}

void ColorLUT::sortStops()
{
	stable_sort( mStops.begin(), mStops.end(),
		[]( const Stop &a, const Stop &b ) { return a.percent < b.percent; }
	);
}

void ColorLUT::fillTable()
{
	if( mStops.size() < 2 ) {
		// fill with only first stop if size = 1, or black if 0
		Colorf col = mStops.size() == 1 ? mStops[0].color : Colorf::black();
		std::fill( mLUT.begin(), mLUT.end(), col );
		return;
	}

	for( size_t s = 0; s < mStops.size() - 1; s++ ) {
		const auto &stop0 = mStops[s];
		const auto &stop1 = mStops[s + 1];

		size_t startIndex = clamp<size_t>( round( stop0.percent * mLUT.size() ), 0, mLUT.size() - 1 );
		size_t stopIndex = clamp<size_t>( round( stop1.percent * mLUT.size() ), 0, mLUT.size() - 1 );


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

} // namespace mason
