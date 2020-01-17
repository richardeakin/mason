/*
Copyright (c) 2019, Richard Eakin
All rights reserved.

This code is designed for use with the Cinder C++ library, http://libcinder.org

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
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

#include "mason/Config.h"
#include "cinder/app/App.h"
#include "cinder/Log.h"

namespace mason {

//! Convenenience routine for common settings that I use with ma::config() within App prepareSettings()
//! - note: I wish I could have placed this method in mason/Config.h alongside the rest of the config stuff,
//!   but well C++ inner classes prevent me from forward declaring App::Settings so we're stuck with this or
//!   other nastier tricks for now
extern void prepareAppSettings( ci::app::App::Settings *settings )
{
	ma::loadConfig();

	auto appConfig = ma::config()->get<ma::Info>( "app" );

	bool fullScreen = appConfig.get<bool>( "fullScreen" );
	bool borderless = appConfig.get<bool>( "borderless", false );
	int screenIndex = appConfig.get<int>( "screenIndex" );
	if( screenIndex >= ci::Display::getDisplays().size() ) {
		CI_LOG_W( "config's app.screenIndex out of range (" << screenIndex << ")" );
		screenIndex = 0;
		fullScreen = false;
		borderless = false;
	}

	const auto &display = ci::Display::getDisplays().at( screenIndex );

	CI_LOG_I( "display name: " << display->getName() << ", size: " << display->getSize() );
	settings->setDisplay( display );


	ci::ivec2 windowPos = appConfig.get<ci::ivec2>( "windowPos", ci::ivec2( 0, 50 ) );
	ci::ivec2 windowSize = appConfig.get<ci::ivec2>( "windowSize", ci::ivec2( 1200, 800 ) );

	if( fullScreen && borderless ) {
		// 'fullscreen + borderless' mode, which overcomes some issues with fullscreen on windows
		windowPos = ci::vec2( 0 );
		windowSize = display->getSize() + ci::ivec2( 0, 1 ); // the size is one pixel larger to fully hide the minimized win task bar
	}
	else {
		settings->setFullScreen( fullScreen );
	}

	settings->setBorderless( borderless );

	//settings->setHighDensityDisplayEnabled();

	settings->setWindowPos( windowPos.x, windowPos.y );
	settings->setWindowSize( windowSize.x, windowSize.y );
}

} // namespace mason
