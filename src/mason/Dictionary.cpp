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

#include "mason/Dictionary.h"

#include "cinder/Utilities.h"
#include "cinder/CinderAssert.h"
#include "jsoncpp/json.h"

using namespace ci;
using namespace std;

using boost::any;

namespace mason {

Dictionary::Dictionary()
{
}

// static
template<typename DataT>
Dictionary	Dictionary::convert( const ci::fs::path &filePath )
{
	return convert<DataT>( loadFile( filePath ) );
}

vector<string> Dictionary::getAllKeys() const
{
	vector<string> result;
	result.reserve( mData.size() );

	for( const auto &mp : mData )
		result.push_back( mp.first );

	return result;
}

bool Dictionary::contains( const std::string &key ) const
{
	return mData.count( key ) != 0;
}

const std::type_info& Dictionary::getType( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw DictionaryExc( "no key named '" + key + "'" );
	}

	return it->second.type();
}

// ----------------------------------------------------------------------------------------------------
// Detail
// ----------------------------------------------------------------------------------------------------

namespace detail {

bool getValue( const boost::any &value, float *result )
{
	if( value.type() == typeid( float ) ) {
		*result = boost::any_cast<float>( value );
	}
	else if( value.type() == typeid( double ) ) {
		*result = static_cast<float>( boost::any_cast<double>( value ) );
	}
	else {
		return false;
	}

	return true;
}

bool getValue( const boost::any &value, double *result )
{
	if( value.type() == typeid( double ) ) {
		*result = boost::any_cast<double>( value );
	}
	else if( value.type() == typeid( float ) ) {
		*result = static_cast<double>( boost::any_cast<float>( value ) );
	}
	else {
		return false;
	}

	return true;
}

bool getValue( const boost::any &value, vector<boost::any> *result )
{
	if( value.type() != typeid( vector<boost::any> ) ) {
		return false;
	}

	*result = boost::any_cast<vector<boost::any>>( value );
	return true;
}

} // namespace mason::detail

// ----------------------------------------------------------------------------------------------------
// Dictionary <-> JSON
// ----------------------------------------------------------------------------------------------------

namespace {

any parseJsonAny( const Json::Value &value );

Dictionary parseJsonDict( const Json::Value &value )
{
	Dictionary result;

	for( auto childIt = value.begin(); childIt != value.end(); ++childIt ) {
		string key = childIt.key().asString();
		any value = parseJsonAny( *childIt );
		result.set( key, value );
	}

	return result;
}

any parseJsonAny( const Json::Value &value )
{
	if( value.isArray() ) {
		vector<any> arr;
		arr.reserve( value.size() );

		for( const auto &child : value )
			arr.push_back( parseJsonAny( child ) );

		return arr;
	}
	else if( value.isObject() ) {
		return parseJsonDict( value );
	}
	else if( value.isBool() ) {
		return value.asBool();
	}
//	else if( value.isUInt() ) {
//		return value.asUInt();
//	}
	else if( value.isInt() ) {
		return value.asInt();
	}
	else if( value.isDouble() ) {
		return value.asDouble();
	}
	else if( value.isString() ) {
		return value.asString();
	}
	else if( value.isNull() ) {
		return nullptr;
	}

	CI_ASSERT_NOT_REACHABLE();
	return nullptr;
}

} // anonymous namespace

// static
template<>
MA_API Dictionary Dictionary::convert<Json::Value>( const Json::Value &data )
{
	Dictionary result =	parseJsonDict( data );
	return result;
}

// static
template<>
MA_API Dictionary Dictionary::convert<Json::Value>( const DataSourceRef &dataSource )
{
	string dataString = loadString( dataSource );

	Json::Reader reader;
	Json::Value data;

	bool success = reader.parse( dataString, data );
	if( ! success ) {
		string errorMessage = reader.getFormattedErrorMessages();
		throw DictionaryExc( "Json::Reader failed to parse source, error message: " + errorMessage );
	}

	return convert( data );
}

} // namespace mason
