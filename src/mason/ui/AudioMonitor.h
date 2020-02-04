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

// TODO SOON: move to mason/mui, document what this visualizes
// - move to mason block too

#pragma once

#include "cinder/Cinder.h"
#include "cinder/audio/Node.h"
#include "cinder/audio/GainNode.h"
#include "cinder/audio/Param.h"

#include "ui/View.h"
#include "ui/Suite.h"
#include "ui/TextManager.h"

namespace cinder {

namespace audio {
	typedef std::shared_ptr<class MonitorNode>	MonitorNodeRef;
}

} // namespace cinder

namespace mason { namespace mui {

typedef std::shared_ptr<class AudioMonitorView> AudioMonitorViewRef;

class AudioMonitorView : public ui::View {
  public:
	AudioMonitorView( const ci::Rectf &bounds = ci::Rectf::zero(), size_t windowSize = 0 );

	void setNode( const ci::audio::NodeRef &node );

	void setParam( const ci::audio::NodeRef &paramOwningNode, ci::audio::Param *param );

	void setMinValue( float value )	{ mMinValue = value; }
	void setMaxValue( float value )	{ mMaxValue = value; }

	void setDrawVolumeMetersEnabled( bool enable )	{ mDrawVolumeMeters = enable; }

  protected:

	void draw( ui::Renderer *ren ) override;

  private:
	void initMonitorNode();
	void drawWaveforms( const ci::audio::Buffer &buffer, const ci::Rectf &rect ) const;
	void drawVolumeMeters( const ci::audio::Buffer &buffer, const ci::Rectf &rect ) const;

	ci::audio::MonitorNodeRef	mMonitorNode;
	ui::TextRef				mLabelText;

	bool			mDrawVolumeMeters = true;
	float			mMinValue = -1;
	float			mMaxValue = 1;
	size_t			mWindowSize = 1024;
};

} // namespace mui

mui::AudioMonitorViewRef addNodeMonitor( const ci::audio::NodeRef &node, const std::string &label = "", size_t windowSize = 0 );
mui::AudioMonitorViewRef addParamMonitor( const ci::audio::NodeRef &paramOwningNode, ci::audio::Param *param, const std::string &label = "", size_t windowSize = 0 );

} // namespace mason
