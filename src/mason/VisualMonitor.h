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

#pragma once

#include "mason/Mason.h"

#include "ui/View.h"
#include "ui/TextManager.h"

#include <vector>

namespace cinder {

namespace gl {
	typedef std::shared_ptr<class TextureFont>	TextureFontRef;
} // namespace cinder::gl

} // namespace cinder

namespace mason {

typedef std::shared_ptr<class VisualMonitorView> VisualMonitorViewRef;

// TODO: move to ma::ui namespace
class MA_API VisualMonitorView : public ui::View {
  public:
	VisualMonitorView( const ci::Rectf &bounds = ci::Rectf::zero(), size_t windowFrames = 0 );

	void setVar( float *var )	{ mVar = var; }

	void setMinValue( float value )	{ mMinValue = value; }
	void setMaxValue( float value )	{ mMaxValue = value; }

	void addTriggerLocation();

  protected:
	void update()	override;
	void draw( ui::Renderer *ren ) override;

  private:
	void drawVar( const ci::Rectf &rect ) const;
	void drawTriggerLines( const ci::Rectf &rect ) const;

	size_t				mWritePos = 0;
	std::vector<float>	mVarRecording;
	std::vector<size_t>	mTriggerLocations;
	float*				mVar = nullptr;

	float			mMinValue = 0;
	float			mMaxValue = 1;

	ui::TextRef	mLabelText;
};

MA_API VisualMonitorViewRef addVarMonitor( float *var, const std::string &label = "", size_t windowSize = 0 );

} // namespace mason
