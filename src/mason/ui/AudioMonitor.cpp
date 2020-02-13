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

#include "mason/ui/AudioMonitor.h"
#include "mason/Hud.h"
#include "vu/TextManager.h"

#include "cinder/gl/gl.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/gl/Shader.h"
#include "cinder/PolyLine.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/MonitorNode.h"
#include "cinder/audio/Utilities.h"
#include "cinder/audio/dsp/Dsp.h"

using namespace std;
using namespace ci;

namespace mason { namespace mui {

//! Outputs the samples from an already evaluated Param to the process Buffer.
class ParamAdapterNode : public audio::Node {
  public:
	ParamAdapterNode( const audio::NodeRef paramOwningNode, audio::Param *param )
		: Node( audio::Node::Format() ), mParamOwningNode( paramOwningNode ), mParam( param )
	{
		setChannelMode( ChannelMode::SPECIFIED );
		setNumChannels( 1 );
	}

	void process( audio::Buffer *buffer ) override
	{
		memmove( buffer->getData(), mParam->getValueArray(), buffer->getNumFrames() * sizeof( float ) );
	}

	audio::NodeRef	mParamOwningNode;
	audio::Param	*mParam;
};

AudioMonitorView::AudioMonitorView( const Rectf &bounds, size_t windowSize )
	: View( bounds )
{
	if( windowSize )
		mWindowSize = windowSize;

	initMonitorNode();
	mLabelText = vu::TextManager::loadText( "Arial", 14 );
}

void AudioMonitorView::initMonitorNode()
{
	auto format = audio::MonitorNode::Format().windowSize( mWindowSize );
	mMonitorNode = audio::master()->makeNode( new audio::MonitorNode( format ) );
}

void AudioMonitorView::setNode( const ci::audio::NodeRef &node )
{
	node >> mMonitorNode;
}

void AudioMonitorView::setParam( const ci::audio::NodeRef &paramOwningNode, ci::audio::Param *param )
{
	mMonitorNode->disconnectAllInputs();

	auto ctx = audio::master();
	auto adapter = ctx->makeNode( new ParamAdapterNode( paramOwningNode, param ) );

	adapter >> mMonitorNode;
}

void AudioMonitorView::draw( vu::Renderer *ren )
{
	if( ! mMonitorNode || ! mLabelText )
		return;

	const auto &buffer = mMonitorNode->getBuffer();

	gl::ScopedGlslProg glslScope( getStockShader( gl::ShaderDef().color() ) );

	float labelY = mLabelText->getAscent() + 2;

	auto waveformBounds = Rectf( 0.0f, labelY, getWidth(), getHeight() );

	// First draw black transparent background
	gl::ScopedColor colorScope( ColorA( 0, 0, 0, 0.5 ) );
	gl::drawSolidRect( waveformBounds );

	if( mDrawVolumeMeters ) {
		const float meterWidth = 20.0f;
		waveformBounds.x2 -= meterWidth;

		auto meterBounds = Rectf( getWidth() - meterWidth, labelY, getWidth(), getHeight() );
		drawVolumeMeters( buffer, meterBounds );
	}

	drawWaveforms( buffer, waveformBounds );

	mLabelText->drawString( getLabel(), vec2( 4, mLabelText->getAscent() ) );
}

// TODO: need to minimize the size of the polyline, average buffer samples according to number of pixels
// - or maybe like 2 samples per pixel to let AA and resizing look nice
void AudioMonitorView::drawWaveforms( const audio::Buffer &buffer, const ci::Rectf &rect ) const
{
	const auto color = ci::ColorA( 0, 0.9f, 0, 1 );

	gl::color( color );

	const float waveHeight = rect.getHeight() / (float)buffer.getNumChannels();
	const float xScale = rect.getWidth() / (float)buffer.getNumFrames();

	float yOffset = rect.y1;
	for( size_t ch = 0; ch < buffer.getNumChannels(); ch++ ) {
		PolyLine2f polyline;
		const float *channel = buffer.getChannel( ch );
		float x = rect.x1;
		for( size_t i = 0; i < buffer.getNumFrames(); i++ ) {
			x += xScale;
			// scale sample from min and max to 0:1
			float scaledSample = lmap<float>( channel[i], mMinValue, mMaxValue, 0, 1 );
			// flip on y axis and fit to wave bounds
			float y = ( 1 - scaledSample ) * waveHeight + yOffset;
			polyline.push_back( vec2( x, y ) );
		}

		if( ! polyline.getPoints().empty() )
			gl::draw( polyline );

		yOffset += waveHeight;
	}

	// draw frame
	gl::color( color.r, color.g, color.b, color.a * 0.6f );
	gl::drawStrokedRect( rect );
}

void AudioMonitorView::drawVolumeMeters( const audio::Buffer &buffer, const ci::Rectf &rect ) const
{
	// TODO: draw one meter per channel
	const float *channel = buffer.getData();

	float rms = audio::dsp::rms( channel, buffer.getSize() );

	// FIXME: this is not at all matching to visuals.
	// - need to approach this with something to compare to, such as in pd and with a simpler test app.
//	float rmsDB = audio::linearToDecibel( rms );
//	float meterPercent = max<float>( 0, lmap<float>( rmsDB, 60, 100, 0, 1 ) );

	float meterPercent = rms;

	Rectf r = rect;
	r.y1 = r.y2 - rect.getHeight() * meterPercent;

	gl::color( 0, 0, 1 );
	gl::drawSolidRect( r );
}

} // namespace mui

// ----------------------------------------------------------------------------------------------------
// Free Functions
// ----------------------------------------------------------------------------------------------------

namespace {
	const Rectf DEFAULT_BOUNDS = Rectf( 10, 10, 210, 90 );
}

mason::mui::AudioMonitorViewRef addNodeMonitor( const ci::audio::NodeRef &node, const string &label, size_t windowSize )
{
	CI_ASSERT( node );

	auto monitorView = make_shared<mason::mui::AudioMonitorView>( DEFAULT_BOUNDS, windowSize );
	monitorView->setNode( node );

	Hud::instance()->addView( monitorView, label );
	return monitorView;
}

mason::mui::AudioMonitorViewRef addParamMonitor( const ci::audio::NodeRef &paramOwningNode, audio::Param *param, const string &label, size_t windowSize )
{
	CI_ASSERT( param );

	auto monitorView = make_shared<mason::mui::AudioMonitorView>( DEFAULT_BOUNDS, windowSize );
	monitorView->setParam( paramOwningNode, param );

	Hud::instance()->addView( monitorView, label );
	return monitorView;
}

} // namespace mason
