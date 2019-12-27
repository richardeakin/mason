/*
Copyright (c) 2018, Richard Eakin - All rights reserved.

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

#include "cinder/Filesystem.h"
#include "cinder/audio/Buffer.h"
#include "cinder/gl/Texture.h"

#if defined( CINDER_IMGUI_BAKED )
// Custom implicit cast operators
#ifndef CINDER_IMGUI_NO_IMPLICIT_CASTS
#define IM_VEC2_CLASS_EXTRA                                             \
ImVec2(const glm::vec2& f) { x = f.x; y = f.y; }                        \
operator glm::vec2() const { return glm::vec2(x,y); }                   \
ImVec2(const glm::ivec2& f) { x = f.x; y = f.y; }                       \
operator glm::ivec2() const { return glm::ivec2(x,y); }

#define IM_VEC4_CLASS_EXTRA                                             \
ImVec4(const glm::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }      \
operator glm::vec4() const { return glm::vec4(x,y,z,w); }               \
ImVec4(const ci::ColorA& f) { x = f.r; y = f.g; z = f.b; w = f.a; }     \
operator ci::ColorA() const { return ci::ColorA(x,y,z,w); }             \
ImVec4(const ci::Color& f) { x = f.r; y = f.g; z = f.b; w = 1.0f; }     \
operator ci::Color() const { return ci::Color(x,y,z); }
#endif

#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

#else
#include "CinderImGui.h"
#endif

#include <vector>

namespace ImGui {

bool Combo( const char* label, int* currIndex, const std::vector<std::string>& values );
bool ListBox( const char* label, int* currIndex, const std::vector<std::string>& values );

#if defined( CINDER_IMGUI_BAKED )
void Image( const ci::gl::Texture2dRef &texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0) );

struct ScopedId : public ci::Noncopyable {
	ScopedId( const std::string &name );
	ScopedId( const void *ptrId );
	ScopedId( const int intId );
	~ScopedId();
};
#endif

} // namespace ImGui

namespace imx {

bool ButtonToggle( const char* label, bool *value, const ImVec2& size = ImVec2( 0,0 ) );
bool FileSelector( const char* label, ci::fs::path* selectedPath, const ci::fs::path &initialPath, const std::vector<std::string> &extensions = {} );
bool WaveformImage( const char* label, const ci::audio::BufferRef &buffer, const ci::gl::Texture2dRef &waveform, int frameMarkerPos = -1, size_t sampleRate = 0 );
//! TODO: rename these to VMeter, add HMeter horizontal versions
void VuMeter( const char* label, const ImVec2& size, float *value, float min = 0, float max = 1 );
void VuMeter( const char* label, const ImVec2& size, float *value, const ImVec4 &fillColor, float min = 0, float max = 1 );
void TexturePreview( const std::string &label, const ci::gl::Texture2dRef &tex, const ci::Rectf &imageBounds, const ImVec2& uv0 = ImVec2(0,1), const ImVec2& uv1 = ImVec2(1,0), const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0) );
bool XYPad( const char *name, const ImVec2& size, float v[2], const ImVec2 &min = ImVec2( 0, 0 ), const ImVec2 &max = ImVec2( 1, 1 ) );

//! If disableInteraction is false, will only dim current drawing scope
// TODO: probably remove once https://github.com/ocornut/imgui/issues/211 is resolved
void BeginDisabled( bool disableInteraction = true );
void EndDisabled();

//! Flashes the ImGui borders red on ma::NOTIFY_FAILURE (log levels > error) and green on ma::NOTIFY_SUCCESS
void SetNotificationColors();


} // namespace imx
