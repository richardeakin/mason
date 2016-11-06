/*
 Copyright (c) 2014, Richard Eakin - All rights reserved.

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

#include "mason/VisualMonitor.h"
#include "mason/Hud.h"
#include "ui/TextManager.h"

#include "cinder/gl/gl.h"
#include "cinder/gl/Shader.h"

using namespace std;
using namespace ci;

namespace mason {

VisualMonitorView::VisualMonitorView( const Rectf &bounds, size_t windowFrames )
	: View( bounds )
{
	if( ! windowFrames )
		windowFrames = 180; // 3 seconds @ 60 fps

	mVarRecording.resize( windowFrames, 0 );

	mLabelText = ui::TextManager::loadText( ui::FontFace::NORMAL, 14 );
	CI_ASSERT( mLabelText );
}

void VisualMonitorView::addTriggerLocation()
{
	// ???: why is this spot on with +2 ?
	mTriggerLocations.push_back( ( mWritePos + 2 ) % mVarRecording.size() );
//	mTriggerLocations.push_back( mWritePos + 0.4 * 60 ); // TEMP: adding look ahead delay manually
}

void VisualMonitorView::update()
{
	if( mWritePos >= mVarRecording.size() ) {
		mWritePos = 0;
		mTriggerLocations.clear();
	}

	mVarRecording[mWritePos++] = *mVar;
}

void VisualMonitorView::draw( ui::Renderer *ren )
{
	const auto color = ci::ColorA( 0, 0.7f, 0.6f, 0.9f );

	gl::ScopedGlslProg glslScope( getStockShader( gl::ShaderDef().color() ) );

	float labelY = mLabelText->getAscent() + 2;
	auto waveformBounds = Rectf( 0.0f, labelY, getWidth(), getHeight() );

	// First draw black transparent background
	gl::ScopedColor colorScope( ColorA( 0, 0, 0, 0.5 ) );
	gl::drawSolidRect( waveformBounds );

	gl::color( color );

	drawVar( waveformBounds );
	drawTriggerLines( waveformBounds );

	// draw frame
	gl::color( ColorA( color.r, color.g, color.b, color.a * 0.6f ) );
	gl::drawStrokedRect( waveformBounds );

	mLabelText->drawString( getLabel(), vec2( 4, mLabelText->getAscent() ) );

	// draw current play position
	float playXPos = waveformBounds.x1 + ( (float)mWritePos / mVarRecording.size() ) * waveformBounds.getWidth();
	gl::drawLine( vec2( playXPos, waveformBounds.y1 ), vec2( playXPos, waveformBounds.y2 ) );
}

void VisualMonitorView::drawVar( const ci::Rectf &rect ) const
{
	const float xScale = rect.getWidth() / (float)mVarRecording.size();

	PolyLine2f waveform;
	float x = rect.x1;
	for( size_t i = 0; i < mVarRecording.size(); i++ ) {
		x += xScale;
		// scale sample from min and max to 0:1
		float scaledSample = lmap<float>( mVarRecording[i], mMinValue, mMaxValue, 0, 1 );
		// flip on y axis and fit to wave bounds
		float y = rect.y1 + ( 1 - scaledSample ) * rect.getHeight();
		waveform.push_back( vec2( x, y ) );
	}

	if( ! waveform.getPoints().empty() )
		gl::draw( waveform );

	// draw frame
	gl::drawStrokedRect( rect );
}

void VisualMonitorView::drawTriggerLines( const Rectf &rect ) const
{
	const float xScale = rect.getWidth() / (float)mVarRecording.size();
	float x = rect.x1;

	gl::ScopedColor colorScope( ColorA::gray( 0.3f, 0.9f ) );

	for( const auto &loc : mTriggerLocations ) {
		float x = rect.x1 + loc * xScale;
		gl::drawLine( vec2( x, rect.y1 ), vec2( x, rect.y2 ) );
	}
}

// ----------------------------------------------------------------------------------------------------
// Free Functions
// ----------------------------------------------------------------------------------------------------

namespace {
	// TODO: figure out way to depict a default view bounds to Hud
	const Rectf DEFAULT_BOUNDS = Rectf( 10, 10, 210, 90 );
}

VisualMonitorViewRef addVarMonitor( float *var, const string &label, size_t windowSize )
{
	auto monitorView = make_shared<VisualMonitorView>( DEFAULT_BOUNDS, windowSize );
	monitorView->setVar( var );

	Hud::instance()->addView( monitorView, label );
	return monitorView;
}

} // namespace mason
