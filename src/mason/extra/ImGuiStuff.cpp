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

#include "mason/extra/ImGuiStuff.h"
#include "mason/Common.h"
#include "mason/Notifications.h"
#include "cinder/audio/Context.h"

#if ! defined( IMGUI_DEFINE_MATH_OPERATORS )
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#if defined( CINDER_IMGUI_BAKED )
#include "imgui/imgui_internal.h" // PushItemFlag( ImGuiItemFlags_Disabled ), ImVec2 operator+
#else
#include "imgui_internal.h"
#endif

using namespace std;
using namespace ci;
using namespace ImGui;

namespace ImGui {

bool vector_getter( void* data, int idx, const char** out_text )
{
	const auto &values = *(const std::vector<std::string> *)data;
	if( idx < 0 || idx >= (int)values.size() )
		return false;

	*out_text = values[idx].c_str();

	return true;
};

bool Combo( const char* label, int* currIndex, const std::vector<std::string>& values )
{
	if( values.empty() )
		return false;

	return Combo( label, currIndex, vector_getter, (void *)( &values ), (int)values.size() );
}

bool ListBox( const char* label, int* currIndex, const std::vector<std::string>& values )
{
	if( values.empty() )
		return false;

	return ListBox( label, currIndex, vector_getter, (void *)( &values ), (int)values.size() );
}

#if defined( CINDER_IMGUI_BAKED )

void Image( const ci::gl::Texture2dRef &texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col )
{
	Image( (void*)(intptr_t) texture->getId(), size, uv0, uv1, tint_col, border_col );
}

ScopedId::ScopedId( const std::string &name )
{
	ImGui::PushID( name.c_str() );
}
ScopedId::ScopedId( const void *ptrId )
{
	ImGui::PushID( ptrId );
}
ScopedId::ScopedId( const int intId )
{
	ImGui::PushID( intId );
}
ScopedId::~ScopedId()
{
	ImGui::PopID();
}

#endif // defined( CINDER_IMGUI_BAKED )

} // namespace ImGui

namespace ImGuiStuff {

namespace {

std::string stripBasePath( const ci::fs::path &fullPath, const ci::fs::path &basePath )
{
	auto fullString = fullPath.generic_string();
	auto baseString = basePath.generic_string();
	auto remainderPos = fullString.find( baseString );

	if( remainderPos == string::npos )
		return fullString;

	return fullString.substr( baseString.size() + 1 );
}

void FileSelectorIterateDirectoryImpl( const fs::path &initialPath, const fs::path &currentPath, fs::path *selectedPath, const std::vector<std::string> &extensions )
{
	for( const auto &entry : fs::directory_iterator( currentPath ) ) {
		const auto &p = entry.path();
		if( fs::is_directory( p ) ) {
			//if( ImGui::TreeNode( stripBasePath( p, initialPath ).c_str() ) ) {
			if( ImGui::TreeNode( p.stem().string().c_str() ) ) {
				FileSelectorIterateDirectoryImpl( initialPath, p, selectedPath, extensions );
				ImGui::TreePop();
			}
		}
		else {
			// if user provided extensions, skip files that aren't in that list
			if( ! extensions.empty() ) {
				string ext = p.extension().string();
				if( ! ext.empty() ) {
					ext.erase( 0, 1 ); // remove the leading "."
				}
				if( find( extensions.begin(), extensions.end(), ext ) == extensions.end() ) {
					continue;
				}
			}
			ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			ImGui::TreeNodeEx( p.filename().string().c_str(), treeNodeFlags );
			if( ImGui::IsItemClicked() ) {
				//CI_LOG_I( "clicked: " << p );
				*selectedPath = p;
			}
		}
	}
}

} // anonymous namespace

bool ButtonToggle( const char* label, bool *value, const ImVec2& size )
{
	bool wasDisabled = ! *value;
	if( wasDisabled ) {
		ImGui::PushStyleColor( ImGuiCol_Button, ImGui::GetStyleColorVec4( ImGuiCol_FrameBg ) );
		ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) );
	}
	else {
		ColorA buttonColor = ImGui::GetStyleColorVec4( ImGuiCol_Button );
		buttonColor.r *= 1.2f;
		buttonColor.g *= 1.2f;
		buttonColor.b *= 1.2f;
		ImGui::PushStyleColor( ImGuiCol_Button, buttonColor );
		ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_DragDropTarget ) );
	}

	bool interacted = ImGui::Button( label, size );
	if( interacted ) {
		*value = ! *value;
	}

	ImGui::PopStyleColor( 2 );

	return interacted;
}

bool FileSelector( const char* label, fs::path* selectedPath, const fs::path &initialPath, const std::vector<std::string> &extensions )
{
	ImGui::PushID( label );

	bool changed = false;

	if( ImGui::Button( "select" ) ) {
		ImGui::OpenPopup( "select file" );
	}

	ImGui::SameLine();

	string writtenStr;
	if( selectedPath->empty() )
		writtenStr = string( "(select " ) + label + ")";
	else {
		writtenStr = stripBasePath( *selectedPath, initialPath );
	}

	if( ImGui::InputText( "##file", &writtenStr, ImGuiInputTextFlags_EnterReturnsTrue ) ) {
		*selectedPath = writtenStr;
		changed = true;
	}

	if( ImGui::BeginPopup( "select file" ) ) {
		fs::path newSelectedPath;
		FileSelectorIterateDirectoryImpl( initialPath, initialPath, &newSelectedPath, extensions );

		if( ! newSelectedPath.empty() ) {
			changed = true;
			*selectedPath = newSelectedPath;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopID();
	return changed;
}

bool WaveformImage( const char* label, const ci::audio::BufferRef &buffer, const ci::gl::Texture2dRef &waveform, int frameMarkerPos, size_t sampleRate )
{
	if( sampleRate == 0 )
		sampleRate = ci::audio::master()->getSampleRate();

	if( frameMarkerPos > 0 ) {
		double currentTime = (double)frameMarkerPos / sampleRate;
		ImGui::Text( "channels: %d, time: %0.3f", buffer->getNumChannels(), currentTime );
	}

	const vec2 screenPos( ImGui::GetCursorScreenPos() );

	auto tintColor = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );
	auto borderColor = ImGui::GetStyleColorVec4( ImGuiCol_FrameBg );
	ImGui::Image( waveform, waveform->getSize(), ivec2( 0, 1 ), ivec2( 1, 0 ), tintColor, borderColor );

	if( frameMarkerPos > 0 ) {
		// draw cursor pos at current playback frame
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		float cursorPos = (float)waveform->getWidth() * (float)frameMarkerPos / (float)buffer->getNumFrames();
		auto cursorCol = ImGui::GetStyleColorVec4( ImGuiCol_SliderGrab );
		draw_list->AddLine( screenPos + vec2( cursorPos, 0 ), screenPos + vec2( cursorPos, waveform->getHeight() ), ImColor( cursorCol ), 3.0f );
	}

	return false;
}

void VuMeter( const char* label, const ImVec2& size, float *value, float min, float max )
{
	//ImVec4 fillColor = Color( ColorModel::CM_HSV, vec3( 0.485f, 0.95f, 0.925f ) ); // neon blue
	ImVec4 fillColor = Color( ColorModel::CM_HSV, vec3( 0.338f, 0.95f, 0.925f ) ); // neon green
	VuMeter( label, size, value, fillColor, min, max );
}

void VuMeter( const char* label, const ImVec2& size, float *value, const ImVec4 &fillColor, float min, float max )
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if( window->SkipItems )
		return;

	const vec2 screenPos( ImGui::GetCursorScreenPos() );

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID( label );

	const ImVec2 label_size = ImGui::CalcTextSize( label, NULL, true );
	const ImRect frame_bb( window->DC.CursorPos, window->DC.CursorPos + size );
	//const ImRect bb( frame_bb.Min, frame_bb.Max + ImVec2( label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f ) );
	//ItemSize( bb, style.FramePadding.y );
	ItemSize( frame_bb, style.FramePadding.y );
	if( ! ItemAdd( frame_bb, id ) )
		return;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const vec2 meterSize = size;

	// TODO: center text if size.x < width
	if( label_size.x > 0.0f ) {
		ImGui::RenderText( screenPos + style.ItemInnerSpacing.x, label ); // use this so stuff after "##" gets hidden
	}

	const float delta = 0.0001f;
	float scaled = clamp<float>( lmap<float>( *value, min, max, size.y, 0 ), 0, size.y );

	if( scaled > delta ) {
		ColorA backgroundColor = ImGui::GetStyleColorVec4( ImGuiCol_FrameBg );
		backgroundColor *= 0.5f;
		draw_list->AddRectFilled( screenPos, screenPos + meterSize, ImColor( backgroundColor ) );
	}

	if( scaled < size.y - delta ) {
		draw_list->AddRectFilled( screenPos + vec2( 0, scaled ), screenPos + meterSize, ImColor( fillColor ) );
	}
}

// TODO: draw label
// TODO: adhere to min and max. currently only defaults are working
// TODO: finish drawing position
//	- rect when inactive, circle when active (like color bar)
bool XYPad( const char *label, const ImVec2& size, float v[2], const ImVec2 &min, const ImVec2 &max )
{
	ImGui::PushID( label );
	ImGui::PushButtonRepeat( true );
	bool active = false;
	float keyReapeatDelay = ImGui::GetIO().KeyRepeatDelay;
	ImGui::GetIO().KeyRepeatDelay = 0;
	if( ImGui::Button( "##Pad", size ) ) {
		vec2 mouse = vec2( ImGui::GetMousePos() ) - vec2( ImGui::GetItemRectMin() );

		// Attempt at drawing crosshairs
		// this is kinda glitchy because it only shows when the mouse button is down. Not bothering with storing these params for when it isn't..
		//ImGui::GetWindowDrawList()->AddLine( { ImGui::GetItemRectMin().x, ImGui::GetMousePos().y }, { ImGui::GetItemRectMax().x, ImGui::GetMousePos().y }, IM_COL32( 255, 255, 0, 255 ), 2 );
		//ImGui::GetWindowDrawList()->AddLine( { ImGui::GetMousePos().x, ImGui::GetItemRectMin().y }, { ImGui::GetMousePos().x, ImGui::GetItemRectMax().y }, IM_COL32( 255, 255, 0, 255 ), 2 );

		vec2 mouseNorm = mouse / vec2( ImGui::GetItemRectSize() );
		v[0] = clamp( mouseNorm.x, min.x, max.x );
		v[1] = clamp( 1 - mouseNorm.y, min.y, max.y );
		active = true;
	}
	ImGui::PopButtonRepeat();
	ImGui::GetIO().KeyRepeatDelay = keyReapeatDelay;

	vec2 valuePos = ImGui::GetItemRectMin();
	valuePos.x += v[0] * size.x;
	valuePos.y += ( 1.0f - v[1] ) * size.y;
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const float rectSize = 6;
	draw_list->AddRectFilled( valuePos - vec2( rectSize / 2 ), valuePos + vec2( rectSize / 2 ), ImColor( ImVec4( 1, 1, 0, 1 ) ) );

	ImGui::PopID();
	return active;
}

void BeginDisabled( bool disableInteraction )
{
	ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
	ImGui::PushItemFlag( ImGuiItemFlags_Disabled, disableInteraction );
}

void EndDisabled()
{
	ImGui::PopItemFlag();
	ImGui::PopStyleVar();
}

void SetNotificationColors()
{
	const double timeShowingFailure = 1.0;
	const double timeShowingSuccess = 1.0;

	static double sTimeLastFailure = -1.0;
	static double sTimeLastSuccess = -1.0;
	static Color sOriginalBorderColor = ImGui::GetStyleColorVec4( ImGuiCol_Border );
	static bool sRegistered = false;

	if( ! sRegistered ) {
		sRegistered = true;
		ma::NotificationCenter::listen( ma::NOTIFY_ERROR, []( const ma::Notification &notification ) {
			// note: don't log from here, it triggers when already logging.
			sTimeLastFailure = ma::currentTime();
		} );
		ma::NotificationCenter::listen( ma::NOTIFY_RESOURCE_RELOADED, []( const ma::Notification &notification ) {
			sTimeLastSuccess = ma::currentTime();
		} );
	}

	double time = ma::currentTime();
	if( time - sTimeLastFailure < timeShowingFailure ) {
		float x = time - sTimeLastFailure;
		auto col = Color( 1, 0, 0 ).lerp( x, sOriginalBorderColor );
		ImGui::GetStyle().Colors[ImGuiCol_Border] = col;
	}	
	else if( time - sTimeLastSuccess < timeShowingSuccess ) {
		float x = time - sTimeLastSuccess;
		auto col = Color( 0, 1, 0 ).lerp( x, sOriginalBorderColor );
		ImGui::GetStyle().Colors[ImGuiCol_Border] = col;
	}
	else {
		ImGui::GetStyle().Colors[ImGuiCol_Border] = sOriginalBorderColor;
	}
}

} // namespace ImGuiStuff
