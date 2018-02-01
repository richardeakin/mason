/*
Copyright (c) 2017, libcinder project - All rights reserved.

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

#include "ui/View.h"
#include "mason/Export.h"

// note: using namespace mason::mui so as not to conflict with ui::*. Oh well..
namespace mason { namespace mui {

using DraggableViewRef = std::shared_ptr<class DraggableView>;


class MA_API DraggableView : public ::ui::View {
  public:
	DraggableView( const ci::Rectf &bounds = ci::Rectf::zero() );

  protected:

	void draw( ::ui::Renderer *ren ) override;

	bool touchesBegan( ci::app::TouchEvent &event )	override;
	bool touchesMoved( ci::app::TouchEvent &event )	override;
	bool touchesEnded( ci::app::TouchEvent &event )	override;

  private:
	ci::vec2	mFirstTouchPos, mInitialPos, mInitialSize;
};

} } // namespace mason::mui
