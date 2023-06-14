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

#include "mason/imx/ImGuiStuff.h"
#include "mason/Common.h"
#include "mason/Notifications.h"
#include "mason/glutils.h"
#include "mason/Profiling.h"

#include "cinder/audio/Context.h"

#if ! defined( IMGUI_DEFINE_MATH_OPERATORS )
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui_internal.h" // PushItemFlag( ImGuiItemFlags_Disabled ), ImVec2 operator+

#include <deque>

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

ScopedItemWidth::ScopedItemWidth( float itemWidth )
{
	ImGui::PushItemWidth( itemWidth );
}
ScopedItemWidth::~ScopedItemWidth()
{
	ImGui::PopItemWidth();
}

} // namespace ImGui

namespace imx {

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
	float scaled = glm::clamp<float>( lmap<float>( *value, min, max, size.y, 0 ), 0, size.y );

	if( scaled > delta ) {
		ColorA backgroundColor = ImGui::GetStyleColorVec4( ImGuiCol_FrameBg );
		backgroundColor *= 0.5f;
		draw_list->AddRectFilled( screenPos, screenPos + meterSize, ImColor( backgroundColor ) );
	}

	if( scaled < size.y - delta ) {
		draw_list->AddRectFilled( screenPos + vec2( 0, scaled ), screenPos + meterSize, ImColor( fillColor ) );
	}
}

void TexturePreview( const std::string &label, const ci::gl::Texture2dRef &tex, const ci::Rectf &imageBounds, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col )
{
	if( ! tex ) {
		ImGui::Text( "%s null", label.c_str() );
		return;
	}

	ImGui::Text( "%s size: [%d, %d], format: %s", label.c_str(), tex->getWidth(), tex->getHeight(), ma::textureFormatToString( tex->getInternalFormat() ) );
	auto fitRect = Rectf( tex->getBounds() ).getCenteredFit( imageBounds, true );
	ImGui::Image( tex, fitRect.getSize(), vec2( 0, 1 ), vec2( 1, 0 ), ColorA( 1, 1, 1, 1 ), ColorA::gray( 0.3f, 1 ) );
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
		v[0] = glm::clamp( mouseNorm.x, min.x, max.x );
		v[1] = glm::clamp( 1 - mouseNorm.y, min.y, max.y );
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

void BeginDisabled( bool disableInteraction, bool grayedOut )
{
	ImGui::PushItemFlag( ImGuiItemFlags_Disabled, disableInteraction );
	ImGui::PushStyleVar( ImGuiStyleVar_Alpha, grayedOut ? ImGui::GetStyle().Alpha * 0.5f : ImGui::GetStyle().Alpha );
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
	Color col = sOriginalBorderColor;
	if( time - sTimeLastFailure < timeShowingFailure ) {
		float x = time - sTimeLastFailure;
		col = Color( 1, 0, 0 ).lerp( x, sOriginalBorderColor );
	}	
	else if( time - sTimeLastSuccess < timeShowingSuccess ) {
		float x = time - sTimeLastSuccess;
		col = Color( 0, 1, 0 ).lerp( x, sOriginalBorderColor );
	}

	auto &colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Border] = col;
	colors[ImGuiCol_Separator] = col;
}

void HoverTooltip( const char* tip, float delay )
{
	if( ImGui::IsItemHovered() && GImGui->HoveredIdTimer > delay ) {
		ImGui::SetTooltip( "%s", tip );
	}
}

// ----------------------------------------------------------------------------------------------------
// Logs
// ----------------------------------------------------------------------------------------------------
// code initially from Simon - thanks! https://gist.github.com/simongeilfus/5ba7528835a9b51b0d0a16e4682c76b4

namespace {

class Logger : public ci::log::Logger {
public:
	Logger()
		: mLevelFilters( { true, true, true, true, true, true } ),
		mMetaFormat( { true, true, false, true } ), 
		mCompact( false ), mCompactEnd( 0 ), mCurrentLineCount( 1 ),
		mAutoScroll( true ), mMaxLines( 10000 ),
		mFilteredLogsCached( false )
	{
		ImVec4* colors = ImGui::GetStyle().Colors;
		mLevelColors = {
			colors[ImGuiCol_FrameBg],
			colors[ImGuiCol_TextDisabled],
			colors[ImGuiCol_NavWindowingDimBg],
			colors[ImGuiCol_NavWindowingHighlight],
			colors[ImGuiCol_HeaderActive],
			colors[ImGuiCol_PlotLinesHovered]
		};
	}

	void clear()
	{
		mCompactEnd = 0;
		mCurrentLineCount = 1;
		mLogs.clear();
		mFilteredLogs.clear();
		mFilteredLogsCached = false;
	}	

	void write( const ci::log::Metadata &meta, const std::string &text ) override
	{
		if( ! mCompact ) {
			mLogs.push_back( Log( meta, text ) );			}
		else {
			bool exists = false;
			for( int i = (int) mLogs.size() - 1; i >= mCompactEnd; --i ) {
				if( mLogs[i].mMetaData.mLocation.getLineNumber() == meta.mLocation.getLineNumber() ) {
					mLogs[i].mAppearance++;
					mLogs[i].mMessage = text;
					mLogs[i].mIsMessageCached = false;

					if( i < mLogs.size() - mCurrentLineCount ) {
						Log log = mLogs[i];
						mLogs.erase( mLogs.begin() + i );
						mLogs.push_back( log );
					}

					exists = true;
					break;
				}
			}
			if( ! exists ) {
				mLogs.push_back( Log( meta, text ) );
			}
		}
		mScrollToBottom = true;
		mFilteredLogsCached = false;
	}

	void draw( const std::string &label )
	{
		if( ImGui::Button( "Clear" ) ) { 
			clear();
		}
		ImGui::SameLine();

		if( mTextFilter.Draw( "Filter", -200.0f ) ) {
			mFilteredLogsCached = false;
		}

		ImGui::SameLine();
		ImGui::PushItemWidth( 75.0f );
		if( ImGui::BeginCombo( "##Options", "Options", ImGuiComboFlags_HeightLarge ) ) {
			bool invalidateCache = false;
			ImGui::Text( "Format" );
			static const std::string metaNames[4] = { "Level", "Function", "Filename", "Line" };
			for( size_t i = 0; i < mMetaFormat.size(); ++i ) {
				invalidateCache |= ImGui::Checkbox( metaNames[i].c_str(), &mMetaFormat[i] );
			}

			ImGui::Separator();
			ImGui::Text( "Options" );
			if( ImGui::Checkbox( "Compact Mode", &mCompact ) ) {
				invalidateCache = true;
				mFilteredLogsCached = false;
				mCompactEnd = static_cast<int>( mLogs.size() );
			}

			ImGui::Checkbox( "Auto Scroll", &mAutoScroll );
			ImGui::Checkbox( "Limit Lines", &mLimitLines );
			if( mAutoScroll ) {
				ImGui::PushItemWidth( 80.0f );
				ImGui::DragInt( "Max Lines", &mMaxLines );
				ImGui::PopItemWidth();
			}

			if( invalidateCache ) {
				for( size_t i = 0; i < mLogs.size(); ++i ) {
					mLogs[i].mIsMessageCached = false;
				}
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::PushItemWidth( 70.0f );
		if( ImGui::BeginCombo( "##Levels", "Levels" ) ) {
			static const std::string levelsNames[6] = { "verbose", "debug", "info", "warning", "error", "fatal" };
			for( size_t i = 0; i < mLevelFilters.size(); ++i ) {
				ImGui::PushID( static_cast<int>( i ) );
				if( ImGui::Checkbox( "##Level", &mLevelFilters[i] ) ) {
					mFilteredLogsCached = false;
				}
				ImGui::SameLine();
				ImGui::ColorEdit3( levelsNames[i].c_str(), &mLevelColors[i].x, ImGuiColorEditFlags_NoInputs );
				ImGui::PopID();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		ImGui::Separator();
		ImGui::BeginChild( "scrolling", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar );

		mCurrentLineCount = 0;

		if( mLimitLines && mLogs.size() > mMaxLines ) {
			mLogs.pop_front();
			if( ! mFilteredLogs.empty() && mFilteredLogs.front() == 0 ) {
				mFilteredLogs.pop_front();
				for( auto &i : mFilteredLogs ) {
					i--;
				}
			}
		}

		if( ! mFilteredLogsCached ) {
			// TODO: implement word-wrap here?
			mFilteredLogs.clear();
			for( size_t i = 0; i < mLogs.size(); i++ ) {
				if( ! mTextFilter.IsActive() || mTextFilter.PassFilter( mLogs[i].mMessageFull.c_str(), mLogs[i].mMessageFull.c_str() + mLogs[i].mMessageFull.length() ) ) {
					if( mLevelFilters[mLogs[i].mMetaData.mLevel] ) {
						mFilteredLogs.push_back( i );
					}
				}
			}
			mFilteredLogsCached = true;
		}

		// TODO: Using clipper and filtering at the same times requires pre-filtering (right above this comment) the
		// list and keeping a cached filtered version of mLogs to be able to pass the actual
		// number of items to the clipper constructor...

		ImGuiListClipper clipper( static_cast<int>( mFilteredLogs.size() ), ImGui::GetTextLineHeightWithSpacing() );
		while( clipper.Step() ) {
			//for( size_t i = 0; i < mLogs.size(); i++ ) {
			for( size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ ) {
				size_t logId = mFilteredLogs[i];
				//if( ! mTextFilter.IsActive() || mTextFilter.PassFilter( mLogs[i].mMessageFull.c_str(), mLogs[i].mMessageFull.c_str() + mLogs[i].mMessageFull.length() ) ) {
				//if( mLevelFilters[mLogs[i].mMetaData.mLevel] ) {
				ImGui::PushStyleColor( ImGuiCol_Text, mLevelColors[mLogs[logId].mMetaData.mLevel] );
				ImGui::TextUnformatted( mLogs[logId].getCached( mMetaFormat ).c_str() );
				ImGui::PopStyleColor();

				// When Compact Mode is enabled, mAppearance will grow for identical logs
				if( mLogs[logId].mAppearance > 1 ) {
					ImGui::SameLine();
					ImGui::PushStyleColor( ImGuiCol_Text, mLevelColors[1] );
					ImGui::Text( "(%i)", (int) mLogs[logId].mAppearance );
					ImGui::PopStyleColor();
				}
				mCurrentLineCount++;
			}
			//}
			//}
		}

		if( mAutoScroll && mScrollToBottom ) {
			ImGui::SetScrollHereY(1.0f);
		}
		mScrollToBottom = false;
		ImGui::EndChild();
	}

protected:

	struct Log {
		Log( const ci::log::Metadata &metaData, const std::string &message ) 
			: mMetaData( metaData ), mMessage( message ), mIsMessageCached( false ), mAppearance( 1 )
		{
			stringstream ss;
			ss << metaData.mLevel;
			ss << metaData.mLocation.getFileName();
			ss << "[" << metaData.mLocation.getLineNumber() << "]";
			ss << " " << metaData.mLocation.getFunctionName();
			ss << " " << message;
			mMessageFull = ss.str();
		}

		string getCached( const std::array<bool,4> &format )
		{
			if( ! mIsMessageCached ) {
				stringstream ss;
				if( format[0] ) ss << mMetaData.mLevel;
				if( format[2] ) {
					if( format[2] ) ss << mMetaData.mLocation.getFileName();
					if( format[3] ) ss << "[" << mMetaData.mLocation.getLineNumber() << "]";
				}
				if( format[1] ) ss << " " << mMetaData.mLocation.getFunctionName();
				if( format[3] && ! format[2] ) ss << "[" << mMetaData.mLocation.getLineNumber() << "]";
				ss << " " << mMessage;

				mMessageCached = ss.str();
				mIsMessageCached = true;
			}

			return mMessageCached;
		}

		uint32_t mAppearance;
		bool mIsMessageCached;
		ci::log::Metadata mMetaData;
		std::string mMessage, mMessageFull, mMessageCached;
	};

	std::deque<Log>	mLogs;
	std::deque<size_t>	mFilteredLogs;
	std::array<bool,6>	mLevelFilters;
	std::array<ImVec4,6> mLevelColors;
	std::array<bool,4>	mMetaFormat;
	bool				mAutoScroll, mScrollToBottom;
	bool				mFilteredLogsCached;
	bool				mCompact; //! Compact Mode - writes same log statements on one line only, with a count after the message
	int					mCompactEnd;
	size_t				mCurrentLineCount;
	bool				mLimitLines;
	int					mMaxLines;

	ImGuiTextFilter     mTextFilter;
};

Logger* getLogger( ImGuiID logId ) 
{
	ImGuiStorage* storage = ImGui::GetStateStorage();
	Logger* appLog = (Logger*) storage->GetVoidPtr( logId );
	if( ! appLog ) {
		auto logger = log::makeLogger<Logger>();
		appLog = logger.get();
		storage->SetVoidPtr( logId, (void*) logger.get() );
	}
	return appLog;
}

} // anonymous namespace

void Logs( const char* label, bool* open )
{
	if( ImGui::Begin( label, open ) ) {
		if( Logger* appLog = getLogger( ImGui::GetID( "Logger::instance" ) ) ) {
			appLog->draw( label );
		}
	}

	ImGui::End();
}

// ----------------------------------------------------------------------------------------------------
// Profiling
// ----------------------------------------------------------------------------------------------------

void Profiling( bool *open )
{
	if( ! Begin( "Profiling", open ) ) {
		End();
		return;
	}

	auto displayTimeFn = []( const pair<string,double> &t ) {
		Text( "%s", t.first.c_str() );
		NextColumn();
		Text( "%6.3f", (float)t.second );
		NextColumn();
	};

	// kludge: working around not all frames updating the gpu profiling the same..
	static std::unordered_map<std::string, double> gpuTimes;

	static Timer  timer{ true };
	static double time = timer.getSeconds();
	const double  elapsed = timer.getSeconds() - time;
	time += elapsed;

	static size_t                      fpsIndex = 0;
	static std::array<float, 180>      fps;
	float currentFps = float( floor( 1.0 / elapsed ) );
	fps[fpsIndex++ % fps.size()] = currentFps;
	if( CollapsingHeader( ( "Framerate (" + to_string( (int)currentFps ) + "s)###fps counter" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
		ImGui::PlotLines( "##fps_lines", fps.data(), int( fps.size() ), 0, 0, 0.0f, 120.0f, ImVec2( ImGui::GetContentRegionAvailWidth(), 90 ) );
	}

	static bool sortTimes = false;
	Checkbox( "sort times", &sortTimes );
	SameLine();
	if( Button( "clear timers" ) ) {
		perf::detail::globalCpuProfiler().getElapsedTimes().clear();
		perf::detail::globalGpuProfiler().getElapsedTimes().clear();
		gpuTimes.clear();
	}

	const float column1Offset = GetWindowWidth() - 110;
	BeginChild( "##Profile Times", vec2( 0, 0 ) );

	if( CollapsingHeader( "cpu (ms)", nullptr, ImGuiTreeNodeFlags_DefaultOpen ) ) {
		Columns( 2, "cpu columns", true );
		SetColumnOffset( 1, column1Offset );
		auto cpuTimes = perf::detail::globalCpuProfiler().getElapsedTimes();

		if( sortTimes ) {
			vector<pair<string, double>> sortedTimes( cpuTimes.begin(), cpuTimes.end() );
			stable_sort( sortedTimes.begin(), sortedTimes.end(), [] ( const auto &a, const auto &b ) { return a.second > b.second; } );
			for( const auto &kv : sortedTimes ) {
				displayTimeFn( kv );
			}
		}
		else {
			for( const auto &kv : cpuTimes ) {
				displayTimeFn( kv );
			}
		}
	}

	Columns( 1 );
	if( CollapsingHeader( "gpu (ms)", nullptr, ImGuiTreeNodeFlags_DefaultOpen ) ) {
		Columns( 2, "gpu columns", true );
		SetColumnOffset( 1, column1Offset );

		auto gpuProfileTimes = perf::detail::globalGpuProfiler().getElapsedTimes();
		for( const auto &kv : gpuProfileTimes ) {
			gpuTimes[kv.first] = kv.second;
		}
		if( sortTimes ) {
			vector<pair<string, double>> sortedTimes( gpuTimes.begin(), gpuTimes.end() );
			stable_sort( sortedTimes.begin(), sortedTimes.end(), [] ( const auto &a, const auto &b ) { return a.second > b.second; } );
			for( const auto &kv : sortedTimes ) {
				displayTimeFn( kv );
			}
		}
		else {
			for( const auto &kv : gpuTimes ) {
				displayTimeFn( kv );
			}
		}
	}

	EndChild();

	End(); // "Profiling"
}

} // namespace imx
