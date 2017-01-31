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
#include "mason/Notifications.h"

#include "cinder/Filesystem.h"
#include "cinder/Log.h"
#include "cinder/System.h"
#include "cinder/AxisAlignedBox.h"
#include "mason/Mason.h"

#include "cinder/Rect.h"
#include "cinder/DataSource.h"
#include "cinder/Rand.h"
#include "cinder/Color.h"

#include <memory>
#include <string>

// For temporary ConnectionList TODO: remove
#include "cinder/Signals.h"

namespace mason {

//! Initializes a bunch of global stuff. If `masonRootDir` is not empty, it is used to initialize file directories used during development.
MA_API void				initialize( const ci::fs::path &masonRootDir = ci::fs::path() );
//! Returns the path the current repositories root folder (walks up from the executable until '.git' is found.
MA_API const ci::fs::path&		getRepoRootPath();
//! Returns a resolved a file path, removing any '..'s if they exist.
MA_API ci::fs::path			normalizePath( const ci::fs::path &path );
//! Sends a global notification that a resource has been loaded. If Hud is in use, this causes the border to flash green.
MA_API void					notifyResourceReloaded();

//! Returns a pointer the global Timeline instance
MA_API ci::Timeline*	timeline();
//! Returns the current time since the app was started
MA_API double			currentTime();

// TODO: move these to mason/Rand.h
MA_API void			initRand( bool randomSeed = false );
MA_API ci::Rand*	rand();
MA_API ci::vec3		randVec3( const ci::AxisAlignedBox &box );
MA_API ci::Color	randColor();

MA_API bool epsilonEqual( const ci::Rectf &r1, const ci::Rectf &r2, float epsilon );

//! Returns a stringified stack trace ready for logging. TODO: move to cinder core
MA_API std::string stackTraceAsString( size_t startingFrame = 0, size_t count = 0, bool skipPlatformFrames = true );

} // namespace mason
