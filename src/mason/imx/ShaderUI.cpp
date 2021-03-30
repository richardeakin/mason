/*
Copyright (c) 2019, Richard Eakin - All rights reserved.

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

#include "mason/Assets.h"
#include "mason/imx/ImGuiStuff.h"

#include "cinder/Log.h"

#include <optional>
#include <variant>

//#define LOG_SHADER_UI( stream )	CI_LOG_I( stream )
#define LOG_SHADER_UI( stream )	((void)0)

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace imx {

namespace {

struct ShaderControl {

	void update();

	ci::gl::GlslProgRef	getShader() const	{ return mShader.lock(); }

	std::weak_ptr<ci::gl::GlslProg>		mShader;
	std::string							mParamLabel;
	std::string							mUniformName;
	std::string							mShaderLine; // this is used to determine if the uniform control params have changed between reloads
	bool								mActive = true; // If inactive, no unform() command will called, to avoid the annoying warning.

	std::optional<float> mMin;
	std::optional<float> mMax;
	std::optional<float> mStep;

	enum class Type {
		Checkbox,
		Slider,
		DragFloat,
		DragFloat2,
		DragFloat3,
		DragFloat4,
		NumTypes
	};

	static const char *typeToString( Type type );

	const char *typeAsString() const	{ return typeToString( mType ); }

	Type mType = Type::NumTypes;

	std::variant<bool, float, vec2, vec3, vec4>	mValue;
	bool mValueNeedsUpdate = false;
};

struct ShaderControlGroup {
	ci::gl::GlslProgRef	getShader() const	{ return mShader.lock(); }

	//! returns true if existing control was found and value was persisted
	bool persistValue( ShaderControl *control ) const;

	std::string						mLabel;
	std::vector<ShaderControl>	mControls;

	std::weak_ptr<ci::gl::GlslProg>	mShader; // used to tell when we should remove the control group
};

class ShaderUIManager {
public:
	static ShaderUIManager* instance();

	void update( bool *open );

private:
	ShaderUIManager();

	void onShaderLoaded( const ci::gl::GlslProgRef &shader, const std::vector<std::pair<ci::fs::path, std::string>> &shaderSources );

	std::vector<ShaderControlGroup>		mGroups;
};

// static
ShaderUIManager* ShaderUIManager::instance()
{
	static ShaderUIManager sInstance;
	return &sInstance;
}
ShaderUIManager::ShaderUIManager()
{
	ma::assets()->getSignalShaderLoaded().connect( ci::signals::slot( this, &ShaderUIManager::onShaderLoaded ) );
}

const std::string whitespaceChars = " \t";

//! Parses a floating point number. Returns true on success.
bool parseFloat( const std::string &valueStr, size_t *pos, float *result )
{
	if( *pos >= valueStr.size() )
		return false;

	size_t posValueBegin = valueStr.find_first_not_of( whitespaceChars, *pos + 1 ); // move forward to first value
	if( posValueBegin == string::npos )
		return false;

	size_t posValueEnd = valueStr.find_first_of( whitespaceChars + ",)", posValueBegin + 1 );
	if( posValueEnd == string::npos )
		return false;

	string valueTrimmed = valueStr.substr( posValueBegin, posValueEnd - posValueBegin );
	try {
		*result = stof( valueTrimmed );
		*pos = posValueEnd + 1;
	}
	catch( std::invalid_argument &exc ) {
		CI_LOG_EXCEPTION( "failed to convert value string '" << valueStr << "' to a float",  exc );
		return false;
	}

	return true;
}

//! Parses an expression in the form of "x = value", where value is a floating point number. Returns true on success.
bool parseFloat( const std::string &varName, const std::string &line, size_t pos, float *result )
{
	size_t posValueBegin = line.find( varName, pos );
	if( posValueBegin == string::npos )
		return false;

	// advance to first non-whitespace char after '='
	posValueBegin = line.find_first_of( "=", posValueBegin );
	if( posValueBegin == string::npos )
		return false;

	posValueBegin = line.find_first_not_of( whitespaceChars, posValueBegin + 1 );
	size_t posValueEnd = line.find_first_of( whitespaceChars + ",", posValueBegin + 1 );
	if( posValueEnd != string::npos )
		posValueEnd = line.size(); // If there was no more whitepsace or comma, set to end of line

	string valueStr = line.substr( posValueBegin, posValueEnd - posValueBegin );

	// TODO: use parseFloat() variant above
	try {
		*result = stof( valueStr );
		return true;
	}
	catch( std::invalid_argument &exc ) {
		CI_LOG_EXCEPTION( "failed to convert value string '" << valueStr << "' to a float",  exc );
	}

	return false;
}

//! Parses a vec2 from string into glm equivalent
void parseVec( const std::string &valueStr, vec2 *result )
{
	size_t pos = valueStr.find_first_of( whitespaceChars + "(", 0 ); // move forward to end of 'vecN('
	if( ! parseFloat( valueStr, &pos, &result->x ) ) {
		*result = vec2( 0 );
	}
	else if( ! parseFloat( valueStr, &pos, &result->y ) ) {
		// Use the one-value constructor
		*result = vec2( result->x );
	}
}

//! Parses a vec3 from string into glm equivalent
void parseVec( const std::string &valueStr, vec3 *result )
{
	size_t pos = valueStr.find_first_of( whitespaceChars + "(", 0 ); // move forward to end of 'vecN('
	if( ! parseFloat( valueStr, &pos, &result->x ) ) {
		*result = vec3( 0 );
	}
	else if( ! parseFloat( valueStr, &pos, &result->y ) ) {
		// Use the one-value constructor
		*result = vec3( result->x );
	}
	else if( ! parseFloat( valueStr, &pos, &result->z ) ) {
		// Parsing x and y succeeded, z failed.
		CI_ASSERT_NOT_REACHABLE();
	}
}

//! Parses a vec4 from string into glm equivalent
void parseVec( const std::string &valueStr, vec4 *result )
{
	size_t pos = valueStr.find_first_of( whitespaceChars + "(", 0 ); // move forward to end of 'vecN('
	if( ! parseFloat( valueStr, &pos, &result->x ) ) {
		*result = vec4( 0 );
	}
	else if( ! parseFloat( valueStr, &pos, &result->y ) ) {
		// Use the one-value constructor
		*result = vec4( result->x );
	}
	else if( ! parseFloat( valueStr, &pos, &result->z ) ) {
		// Parsing x and y succeeded, z failed.
		CI_ASSERT_NOT_REACHABLE();
	}
	else if( ! parseFloat( valueStr, &pos, &result->w ) ) {
		// Parsing x and y succeeded and z succeeded, w failed.
		CI_ASSERT_NOT_REACHABLE();
	}
}

void ShaderUIManager::onShaderLoaded( const ci::gl::GlslProgRef &shader, const std::vector<std::pair<ci::fs::path, std::string>> &shaderSources )
{
	LOG_SHADER_UI( "shader label: " << shader->getLabel() );

	ShaderControlGroup group;
	group.mShader = shader;

	// takie either the GlslProg's label or the last source filename that we process as the group as label
	// TODO: consider making the name from all shader names (eg. "sceneObject.vert, sceneObject.frag")
	group.mLabel = ! shader->getLabel().empty() ? shader->getLabel() : shaderSources.back().first.filename().string();

	auto oldGroupIt = find_if( mGroups.begin(), mGroups.end(),
		[&group]( const auto &other ) {
			return group.mLabel == other.mLabel;
		}
	);

	for( const auto &sp : shaderSources ) {
		bool hudDisabledForSource = false;
		bool inDisabledBlock = false;

		// search for lines with "ui:" that begin with "uniform"
		istringstream input( sp.second );
		string line;
		while( getline( input, line ) ) {
			if( hudDisabledForSource )
				break;

			size_t posHudStr = line.find( "ui:" );
			if( posHudStr == string::npos )
				continue;

			if( inDisabledBlock ) {
				// check for disable block close
				size_t posDisableBracketBlock = line.find( "ui: }" );
				if( posDisableBracketBlock != string::npos ) {
					LOG_SHADER_UI( "disable block end" );
					inDisabledBlock = false;
				}
				else {
					LOG_SHADER_UI( "\t.. skipping line in disable block: " << line );
					continue;
				}
			}

			// check for 'disable' keywords
			size_t posDisable = line.find( "ui: disable" );
			if( posDisable != string::npos ) {
				// check for 'ui: disable' at the beginning of a line
				if( posDisable <= 3 ) {

					// check for a disable block, with string "ui: disable {".
					// Later the group should end with a "ui: }" string
					size_t posDisableBracketOpen = line.find( '{', posDisable + 12 ); // 12 = strlen( "ui: disable" )
					if( posDisableBracketOpen != string::npos ) {
						LOG_SHADER_UI( "disable block begin" );
						inDisabledBlock = true;
						continue;
					}
					else {
						// this is a global disable for the current shader source
						LOG_SHADER_UI( "shader controls disabled for shader source: " << sp.first );
						hudDisabledForSource = true;
						break;
					}
				}
				else {
					LOG_SHADER_UI( "\t.. skipping disabled line: " << line );
					continue;
				}
			}

			size_t posFoundUniformStr = line.find( "uniform" );
			// don't evaluate uniforms that aren't at the beginning of the line, simple way to allow them to be commented out
			if( posFoundUniformStr != 0 )
				continue;

			// parse uniform type
			size_t posTypeBegin = posFoundUniformStr + 7; // advance to end of "uniform"
			posTypeBegin = line.find_first_not_of( whitespaceChars, posTypeBegin ); // find next non-whitespace char
			size_t posTypeEnd = line.find_first_of( whitespaceChars, posTypeBegin + 1 ); // find next whitespace char
			if( posTypeBegin == string::npos || posTypeEnd == string::npos ) {
				CI_LOG_E( "could not parse param type" );
				continue;
			}

			string paramType = line.substr( posTypeBegin, posTypeEnd - posTypeBegin );

			// parse uniform name.
			// TODO: allow this to be an alternate name
			// - means dropping the requirement for ':' and 'hud' being the end of the line
			size_t posUniformNameBegin = line.find_first_not_of( whitespaceChars, posTypeEnd + 1 );
			size_t posUniformNameEnd = line.find_first_of( whitespaceChars + ";", posUniformNameBegin + 1 );
			string uniformName = line.substr( posUniformNameBegin, posUniformNameEnd - posUniformNameBegin );

			// mark whether uniform is inactive - we'll still leave a control so that it's value persist but it won't do anything
			bool active = false;
			for( const auto &uniform : shader->getActiveUniforms() ) {
				if( uniform.getName() == uniformName ) {
					active = true;
					break;
				}
			}

			// parse param label, which directly follows 'ui:" in quotes
			size_t posLabelBegin = line.find( "\"", posFoundUniformStr );
			size_t posLabelEnd = line.find( "\"", posLabelBegin + 1 );

			if( posLabelBegin == string::npos || posLabelEnd == string::npos ) {
				CI_LOG_E( "could not parse param label" );
				continue;
			}

			string paramLabel = line.substr( posLabelBegin + 1, posLabelEnd - posLabelBegin - 1 );
			LOG_SHADER_UI( "- param: " << paramLabel << ", uniform: " << uniformName << ", type: " << paramType );

			// parse default value after type, var name and '='
			string defaultValue;
			size_t posDefaultValueBegin = line.find( "=", posTypeEnd );
			if( posDefaultValueBegin != string::npos && posDefaultValueBegin < posHudStr ) {
				posDefaultValueBegin = line.find_first_not_of( whitespaceChars, posDefaultValueBegin + 1 ); // skip whitespace
				size_t posDefaultValueEnd = line.find_first_of( ";", posDefaultValueBegin + 1 );
				defaultValue = line.substr( posDefaultValueBegin, posDefaultValueEnd - posDefaultValueBegin );
				LOG_SHADER_UI( "\tdefault value: " << defaultValue );
			}

			// parse min and max for controls that can use them
			ShaderControl control;
			control.mShader = shader;
			control.mUniformName = uniformName;
			control.mShaderLine = line;
			control.mActive = active; // TODO: remove mActive if not needed for imgui
			control.mParamLabel = paramLabel;

			float minValue, maxValue, stepValue;

			bool hasMinValue = parseFloat( "min", line, posLabelEnd, &minValue );
			if( hasMinValue ) {
				LOG_SHADER_UI( "\tmin value: " << minValue );
				control.mMin = minValue;
			}
			bool hasMaxValue = parseFloat( "max", line, posLabelEnd, &maxValue );
			if( hasMaxValue ) {
				LOG_SHADER_UI( "\tmax value: " << maxValue );
				control.mMax = maxValue;
			}
			bool hasStepValue = parseFloat( "step", line, posLabelEnd, &stepValue );
			if( hasStepValue ) {
				LOG_SHADER_UI( "\tstep value: " << stepValue );
				control.mStep = stepValue;
			}

			if( paramType == "bool" ) {
				control.mType = ShaderControl::Type::Checkbox;
				if( oldGroupIt == mGroups.end() || ! oldGroupIt->persistValue( &control ) ) {
					control.mValue = defaultValue == "true" ? true : false;
					control.mValueNeedsUpdate = true;
				}
			}
			else if( paramType == "float" ) {
				// TODO: parse out a "type = slider" string, and if that exists use a slider instead of numbox
				string controlType = "";
				//controlType = "slider";
				if( controlType == "slider" ) {
					control.mType = ShaderControl::Type::Slider;
				}
				else {
					control.mType = ShaderControl::Type::DragFloat;
				}

				if( oldGroupIt == mGroups.end() || ! oldGroupIt->persistValue( &control ) ) {
					control.mValue = ( ! defaultValue.empty() ) ? stof( defaultValue ) : 0;
					control.mValueNeedsUpdate = true;
				}
			}
			else if( paramType == "vec2" ) {
				control.mType = ShaderControl::Type::DragFloat2;
				if( oldGroupIt == mGroups.end() || ! oldGroupIt->persistValue( &control ) ) {
					vec2 initialValue;
					if( ! defaultValue.empty() )
						parseVec( defaultValue, &initialValue );

					control.mValue = initialValue;
					control.mValueNeedsUpdate = true;
				}
			}
			else if( paramType == "vec3" ) {
				control.mType = ShaderControl::Type::DragFloat3;
				if( oldGroupIt == mGroups.end() || ! oldGroupIt->persistValue( &control ) ) {
					vec3 initialValue;
					if( ! defaultValue.empty() )
						parseVec( defaultValue, &initialValue );

					control.mValue = initialValue;
					control.mValueNeedsUpdate = true;
				}
			}
			else if( paramType == "vec4" ) {
				control.mType = ShaderControl::Type::DragFloat4;
				if( oldGroupIt == mGroups.end() || ! oldGroupIt->persistValue( &control ) ) {
					vec4 initialValue;
					if( ! defaultValue.empty() )
						parseVec( defaultValue, &initialValue );

					control.mValue = initialValue;
					control.mValueNeedsUpdate = true;
				}
			}

			group.mControls.push_back( control );
		}
	}

	LOG_SHADER_UI( "adding group with label: " << group.mLabel << ", controls: " << group.mControls.size() );

	if( oldGroupIt != mGroups.end() ) {
		LOG_SHADER_UI( "...erasing old group..." );
		mGroups.erase( oldGroupIt );
	}

	mGroups.push_back( group );
}

void ShaderUIManager::update( bool *open )
{
	// remove any ShaderControlGroups that have an expired shader
	mGroups.erase( remove_if( mGroups.begin(), mGroups.end(),
		[]( const ShaderControlGroup &group ) {
			bool shaderExpired = ! ( group.getShader() );
			if( shaderExpired ) {
				LOG_SHADER_UI( "removing group group for expired shader: " << group.mLabel );
			}

			return shaderExpired;
		}
	), mGroups.end() );

	if( im::Begin( "ShaderUI", open ) ) {
		im::Text( "shaders: %d", mGroups.size() );

		// TODO: want to show more info about shaders, which is why I added the option to 'show all' even when there are no controls
		// - though most info seems to be in the GlslProg::Format, eg. defines, shader names. Might need to pass that through
		static bool showAll = false;
		im::Checkbox( "show all", &showAll );

		for( auto &group : mGroups ) {
			if( ! showAll && group.mControls.empty() ) {
				continue;
			}
			if( im::CollapsingHeader( group.mLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
				auto shader = group.mShader.lock();
				im::Text( "label: %s", shader->getLabel().c_str() );
				for( auto &control : group.mControls ) {
					control.update();
				}
			}
		}
	}

	im::End();
}

// add ImGui widgets based on type / variant
void ShaderControl::update()
{
	float v_speed = mStep.value_or( 1.0f );
	float v_min = mMin.value_or( 0.0f );
	float v_max = mMax.value_or( 0.0f );
	if( mType == ShaderControl::Type::Checkbox ) {
		auto &value = std::get<bool>( mValue );
		if( im::Checkbox( mParamLabel.c_str(), &value ) || mValueNeedsUpdate ) {
			mValue = value;
			mShader.lock()->uniform( mUniformName, value );
		}
	}
	else if( mType == ShaderControl::Type::DragFloat ) {
		auto &value = std::get<float>( mValue );
		if( im::DragFloat( mParamLabel.c_str(), &value, v_speed, v_min, v_max ) || mValueNeedsUpdate ) {
			mShader.lock()->uniform( mUniformName, value );
		}
	}
	else if( mType == ShaderControl::Type::DragFloat2 ) {
		auto &value = std::get<vec2>( mValue );
		if( im::DragFloat2( mParamLabel.c_str(), &value.x, v_speed, v_min, v_max ) || mValueNeedsUpdate ) {
			mShader.lock()->uniform( mUniformName, value );
		}
	}
	else if( mType == ShaderControl::Type::DragFloat3 ) {
		auto &value = std::get<vec3>( mValue );
		if( im::DragFloat3( mParamLabel.c_str(), &value.x, v_speed, v_min, v_max ) || mValueNeedsUpdate ) {
			mShader.lock()->uniform( mUniformName, value );
		}
	}
	else if( mType == ShaderControl::Type::DragFloat4 ) {
		auto &value = std::get<vec4>( mValue );
		if( im::DragFloat4( mParamLabel.c_str(), &value.x, v_speed, v_min, v_max ) || mValueNeedsUpdate ) {
			mShader.lock()->uniform( mUniformName, value );
		}
	}
	else {
		im::Text( "label: %s, type: %s", mParamLabel.c_str(), typeAsString() );
	}

	mValueNeedsUpdate = false;
}

bool ShaderControlGroup::persistValue( ShaderControl *control ) const
{
	// If there is a matching old control, copy over it's value
	auto oldControlIt = find_if( mControls.begin(), mControls.end(),
		[control]( const auto &other ) {
		return control->mParamLabel == other.mParamLabel && control->mType == other.mType;
	}
	);

	if( oldControlIt != mControls.end() ) {
		control->mValue = oldControlIt->mValue;
		control->mValueNeedsUpdate = true;
		return true;
	}
	else {
		return false;
	}
}

// static
const char *ShaderControl::typeToString( Type type )
{
	switch( type ) {
		case Type::Checkbox:	return "Checkbox";
		case Type::Slider:		return "Slider";
		case Type::DragFloat:	return "DragFloat";
		case Type::DragFloat2:	return "DragFloat2";
		case Type::DragFloat3:	return "DragFloat3";
		case Type::DragFloat4:	return "DragFloat4";
		case Type::NumTypes:	return "NumTypes";
	}

	CI_ASSERT_NOT_REACHABLE();
	return "(unknown)";
}

} // anon

void InitShaderUI()
{
	ShaderUIManager::instance();
}

void ShaderUI( bool *open )
{
	ShaderUIManager::instance()->update( open );
}

} // namespace imx
