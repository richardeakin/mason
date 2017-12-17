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

#include "mason/ui/DraggableView.h"
#include "cinder/Log.h"

using namespace ci;
using namespace std;

namespace mason { namespace mui {

DraggableView::DraggableView( const ci::Rectf &bounds )
{
}

void DraggableView::draw( ::ui::Renderer *ren )
{
	//ren->setColor( Color( 0, 0, 1 ) );
	//ren->drawStrokedRect( getBoundsLocal(), 3 );
}

bool DraggableView::touchesBegan( app::TouchEvent &event )
{
	auto &firstTouch = event.getTouches().front();
	mFirstTouchPos = firstTouch.getPos();
	mInitialPos = getPos();
	mInitialSize = getSize();

	//CI_LOG_I( "touch: " << mFirstTouchPos << ", mInitialPos: " << mInitialPos << ", mInitialSize: " << mInitialSize );

	firstTouch.setHandled();
	return true;
}

bool DraggableView::touchesMoved( app::TouchEvent &event )
{
	vec2 touchPos = event.getTouches().front().getPos();
	vec2 deltaTouchPos = touchPos - mFirstTouchPos;
	vec2 deltaViewPos = mInitialPos + deltaTouchPos;

	//CI_LOG_I( "touch pos: " << touchPos << ", deltaTouchPos: " << deltaTouchPos << ", deltaViewPos: " << deltaViewPos << ", current view pos: " << getPos() );

	setPos( deltaViewPos );

	return true;
}

bool DraggableView::touchesEnded( app::TouchEvent &event )
{
	return true;
}

} } // namespace mason::mui
