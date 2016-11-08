/*
Copyright (c) 2015, The Barbarian Group
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

#include "cinder/DataSource.h"
#include "cinder/DataTarget.h"
#include "cinder/System.h"
#include "cinder/Vector.h"
#include "cinder/Color.h"
#include "cinder/Noncopyable.h"
#include "cinder/CinderAssert.h"

#include "jsoncpp/json.h"

namespace mason {

class ConfigExc : public ci::Exception {
  public:
	ConfigExc( const std::string &description )
		: Exception( description )
	{}
	ConfigExc( const char *mangledName, const std::string &category, const std::string &key )
		: Exception( "could not parse value of type '" + ci::System::demangleTypeName( mangledName ) + "', for category: " + category + ", key: " + key )
	{
	}
};

typedef std::shared_ptr<class Config> ConfigRef;

class Config : private ci::Noncopyable {
public:
	class Options {
		friend class Config;
	public:
		Options() : mSetIfDefault( false ), mWriteIfSet( false ) {}

		//! If \a enabled, add the default value to the configuration file if the key was not found.
		Options& setIfDefault( bool enabled = true ) { mSetIfDefault = enabled; return *this; }
		//! If \a enabled, write the config file immediately after setting the new value.
		Options& writeIfSet( bool enabled = true ) { mWriteIfSet = enabled; return *this; }

	private:
		bool  mSetIfDefault;
		bool  mWriteIfSet;
	};

public:
	~Config() {}

	//! Returns the singleton instance of this app's Config
	static Config* instance();

	void setOptions( const Options &options ) { mOptions = options; }

	bool read( const ci::DataSourceRef &source );
	bool write()	{ return write( mTarget ); }
	bool write( const ci::DataTargetRef &target );

	template<typename T>
	T get( const std::string &category, const std::string &key )
	{
		const auto &value = mRoot[category][key];

		T result;
		if( ! getValue( value, &result ) ) {
			throw ConfigExc( typeid( result ).name(), category, key );
		}

		return result;
	}

	template<typename T>
	T get( const std::string &category, const std::string &key, const T &defaultValue )
	{
		const auto &value = mRoot[category][key];

		T result;
		if( ! getValue( value, &result ) ) {
			result = defaultValue;

			if( mOptions.mSetIfDefault )
				set( category, key, defaultValue );
		}

		return result;
	}

	template<typename T>
	bool set( const std::string &category, const std::string &key, const T &value )
	{
		mRoot[category][key] = Json::Value( value );
		mDirty = true;

		if( mOptions.mWriteIfSet )
			write();

		return true;
	}

	//! Returns true if result was properly filled, false otherwise
	template<typename T>
	bool getValue( const Json::Value &value, T *result ) { return false; }

	template<typename T>
	std::vector<T>	getList( const std::string &category );

	const Json::Value&	getGroup( const std::string &category ) const	{ return mRoot[category]; }

private:
	Config() : mDirty( false ) {}
	Config( const Options &options ) : mDirty( false ), mOptions( options ) {}

	bool               mDirty;
	Options            mOptions;

	Json::Value        mRoot;

	ci::DataSourceRef  mSource;
	ci::DataTargetRef  mTarget;
};

template<>
inline bool Config::getValue<bool>( const Json::Value &value, bool *result )
{
	if( ! value.isBool() )
		return false;

	*result = value.asBool();
	return true;
}

template<>
inline bool Config::getValue<int>( const Json::Value &value, int *result )
{
	if( ! value.isInt() )
		return false;

	*result = value.asInt();
	return true;
}

template<>
inline bool Config::getValue<uint16_t>( const Json::Value &value, uint16_t *result )
{
	if( ! value.isInt() )
		return false;

	*result = value.asUInt();
	return true;
}

template<>
inline bool Config::getValue<uint32_t>( const Json::Value &value, uint32_t *result )
{
	if( ! value.isInt() )
		return false;

	*result = value.asUInt();
	return true;
}

template<>
inline bool Config::getValue<uint64_t>( const Json::Value &value, uint64_t *result )
{
	if( ! value.isInt() )
		return false;

	*result = value.asUInt64();
	return true;
}

template<>
inline bool Config::getValue<unsigned long>( const Json::Value &value, unsigned long *result )
{
	if( ! value.isInt() )
		return false;

	*result = static_cast<unsigned long>( value.asUInt64() );
	return true;
}


template<>
inline bool Config::getValue<float>( const Json::Value &value, float *result )
{
	// convert any numerical value to float
	if( ! value.isNumeric() )
		return false;

	*result = static_cast<float>( value.asDouble() );
	return true;
}

template<>
inline bool Config::getValue<double>( const Json::Value &value, double *result )
{
	// convert any numerical value to float
	if( ! value.isNumeric() )
		return false;

	*result = value.asDouble();
	return true;
}

template<>
inline bool Config::getValue<std::string>( const Json::Value &value, std::string *result )
{
	if( ! value.isString() )
		return false;

	*result = value.asString();
	return true;
}

template<>
inline bool Config::getValue<ci::ivec2>( const Json::Value &value, ci::ivec2 *result )
{
	if( ! value.isArray() || value.size() < 2 )
		return false;

	bool success = getValue<int>( value[0], &result->x );
	success		|= getValue<int>( value[1], &result->y );
	return success;
}

template<>
inline bool Config::getValue<ci::ivec3>( const Json::Value &value, ci::ivec3 *result )
{
	if( ! value.isArray() || value.size() < 3 )
		return false;

	bool success = getValue<int>( value[0], &result->x );
	success		|= getValue<int>( value[1], &result->y );
	success		|= getValue<int>( value[2], &result->z );
	return success;
}


template<>
inline bool Config::getValue<ci::vec2>( const Json::Value &value, ci::vec2 *result )
{
	if( ! value.isArray() || value.size() < 2 )
		return false;

	bool success = getValue<float>( value[0], &result->x );
	success		|= getValue<float>( value[1], &result->y );
	return success;
}

template<>
inline bool Config::getValue<ci::vec3>( const Json::Value &value, ci::vec3 *result )
{
	if( ! value.isArray() || value.size() < 3 )
		return false;

	bool success = getValue<float>( value[0], &result->x );
	success		|= getValue<float>( value[1], &result->y );
	success		|= getValue<float>( value[2], &result->z );
	return success;
}

template<>
inline bool Config::getValue<ci::vec4>( const Json::Value &value, ci::vec4 *result )
{
	if( ! value.isArray() || value.size() < 4 )
		return false;

	bool success = getValue<float>( value[0], &result->x );
	success		|= getValue<float>( value[1], &result->y );
	success		|= getValue<float>( value[2], &result->z );
	success		|= getValue<float>( value[3], &result->w );
	return success;
}

template<>
inline bool Config::getValue<ci::Color>( const Json::Value &value, ci::Color *result )
{
	if( ! value.isArray() || value.size() < 3 )
		return false;

	bool success = getValue<float>( value[0], &result->r );
	success		|= getValue<float>( value[1], &result->g );
	success		|= getValue<float>( value[2], &result->b );
	return success;
}

template<>
inline bool Config::getValue<ci::ColorA>( const Json::Value &value, ci::ColorA *result )
{
	if( ! value.isArray() || value.size() < 3 )
		return false;

	bool success = getValue<float>( value[0], &result->r );
	success		|= getValue<float>( value[1], &result->g );
	success		|= getValue<float>( value[2], &result->b );
	success		|= getValue<float>( value[3], &result->a );
	return success;
}

template<>
inline bool Config::getValue<std::vector<std::string>>( const Json::Value &value, std::vector<std::string> *result )
{
	if( ! value.isArray() )
		return false;

	CI_ASSERT( result->empty() );

	for( auto childIt = value.begin(); childIt != value.end(); ++childIt ) {
		result->push_back( childIt->asString() );
	}

	return true;
}

template<>
inline bool Config::set<ci::ivec2>( const std::string &category, const std::string &key, const ci::ivec2 &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.x;
	array[1] = value.y;

	return set( category, key, array );
}
	
template<>
inline bool Config::set<ci::ivec3>( const std::string &category, const std::string &key, const ci::ivec3 &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.x;
	array[1] = value.y;
	array[2] = value.z;

	return set( category, key, array );
}

template<>
inline bool Config::set<ci::vec2>( const std::string &category, const std::string &key, const ci::vec2 &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.x;
	array[1] = value.y;

	return set( category, key, array );
}
	
template<>
inline bool Config::set<ci::vec3>( const std::string &category, const std::string &key, const ci::vec3 &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.x;
	array[1] = value.y;
	array[2] = value.z;

	return set( category, key, array );
}
	
template<>
inline bool Config::set<ci::vec4>( const std::string &category, const std::string &key, const ci::vec4 &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.x;
	array[1] = value.y;
	array[2] = value.z;
	array[3] = value.z;

	return set( category, key, array );
}

template<>
inline bool Config::set<ci::Color>( const std::string &category, const std::string &key, const ci::Color &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.r;
	array[1] = value.g;
	array[2] = value.b;

	return set( category, key, array );
}

template<>
inline bool Config::set<ci::ColorA>( const std::string &category, const std::string &key, const ci::ColorA &value )
{
	Json::Value array( Json::arrayValue );
	array[0] = value.r;
	array[1] = value.g;
	array[2] = value.b;
	array[3] = value.a;

	return set( category, key, array );
}

template<>
inline bool Config::set<std::vector<std::string>>( const std::string &category, const std::string &key, const std::vector<std::string> &value )
{
	Json::Value array( Json::arrayValue );
	for( unsigned int i = 0; i < value.size(); i++ ) {
		array[i] = value[i];
	}

	return set( category, key, array );
}


template<typename T>
std::vector<T> Config::getList( const std::string &category )
{
	std::vector<T> result;
	for( const auto &elem : mRoot[category] ) {
		T t;
		if( getValue( elem, &t ) ) {
			result.push_back( t );
		}
	}

	return result;
}

static inline Config* config() { return Config::instance(); }

} // namespace mason
