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

#include "mason/Info.h"

#include "cinder/Utilities.h"
#include "cinder/CinderAssert.h"
#include "cinder/CinderMath.h"
#include "jsoncpp/json.h"

using namespace ci;
using namespace std;

namespace mason {

Info::Info()
{
}

// static
template<typename DataT>
Info	Info::convert( const ci::fs::path &filePath )
{
	return convert<DataT>( loadFile( filePath ) );
}

vector<string> Info::getAllKeys() const
{
	vector<string> result;
	result.reserve( mData.size() );

	for( const auto &mp : mData )
		result.push_back( mp.first );

	return result;
}

bool Info::contains( const std::string &key ) const
{
	return mData.count( key ) != 0;
}

const std::type_info& Info::getType( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw InfoExc( "no key named '" + key + "'" );
	}

	return it->second.type();
}

void Info::merge( const Info &other )
{
	for( const auto& mp : other.getData() ) {
		const string &key = mp.first;
		if( mp.second.type() == typeid( Info ) ) {
			// if we have that key, check if it is also a Info.
			// - if yes then recursive merge. if no, blow away our current contents with other's.
			const Info *b = any_cast<Info>( &mp.second );
			auto it = mData.find( mp.first );
			if( it != mData.end() && it->second.type() == typeid( Info ) ) {				
				Info *a = any_cast<Info>( &it->second );
				a->merge( *b );
			}
			else {
				mData[key] = mp.second;
			}
		}
		//else if( mp.second.type() == typeid( std::vector<any> ) ) {
		//	// iterate over each array type to look for Dictionaries or values
		//	// - check if we have a key and it is also an array.
		//	// - if yes then merge the two, if no then replace our key
		//	const vector<any> *b = any_cast<vector<any>>( &mp.second );
		//	auto it = mData.find( mp.first );
		//	if( it != mData.end() && it->second.type() == typeid( std::vector<any> ) ) {
		//		vector<any> *a = any_cast<vector<any>>( &it->second );
		//		for( const auto &c : *b ) {

		//		}
		//	}
		//	else {
		//		mData[key] = b;
		//	}
		//}
		else {
			mData[key] = mp.second;
		}
	}
}

const Info::Value& Info::operator[]( const std::string &key ) const
{
	return getStrict<Info::Value>( key );
}

Info::Value& Info::operator[]( const std::string &key )
{
	return mData[key];
}

std::string	Info::toString() const
{
	stringstream ss;
	ss << *this;
	return ss.str();
}


template<>
const Info::Value& Info::getStrict<Info::Value>( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw InfoExc( "no key named '" + key + "'" );
	}

	return it->second;
}

// ----------------------------------------------------------------------------------------------------
// Detail
// ----------------------------------------------------------------------------------------------------

namespace detail {

bool getValue( const Info::Value &value, float *result )
{
	const auto castedFloat = any_cast<float>( &value );
	if( castedFloat ) {
		*result = *castedFloat;
		return true;
	}

	const auto castedDouble = any_cast<double>( &value );
	if( castedDouble ) {
		*result = static_cast<float>( *castedDouble );
		return true;
	}

	const auto castedInt = any_cast<int>( &value );
	if( castedInt ) {
		*result = static_cast<float>( *castedInt );
		return true;
	}

	return false;
}

bool getValue( const Info::Value &value, double *result )
{
	const auto castedDouble = any_cast<double>( &value );
	if( castedDouble ) {
		*result = *castedDouble;
		return true;
	}

	const auto castedFloat = any_cast<float>( &value );
	if( castedFloat ) {
		*result = static_cast<double>( *castedFloat );
		return true;
	}

	const auto castedInt = any_cast<int>( &value );
	if( castedInt ) {
		*result = static_cast<double>( *castedInt );
		return true;
	}

	return false;
}

bool getValue( const Info::Value &value, size_t *result )
{
	const auto castedInt = any_cast<int>( &value );
	if( castedInt ) {
		*result = static_cast<size_t>( *castedInt );
		return true;
	}
	return false;
}

bool MA_API getValue( const Info::Value &value, int32_t *result )
{
	const auto castedInt = any_cast<int>( &value );
	if( castedInt ) {
		*result = static_cast<int32_t>( *castedInt );
		return true;
	}
	return false;
}

bool MA_API getValue( const Info::Value &value, uint32_t *result )
{
	const auto castedInt = any_cast<int>( &value );
	if( castedInt ) {
		*result = static_cast<uint32_t>( *castedInt );
		return true;
	}
	return false;
}

bool getValue( const Info::Value &value, vec2 *result )
{
	if( value.type() == typeid( vec2 ) ) {
		*result = any_cast<vec2>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>>( &value );
	if( ! castedVector || castedVector->size() < 2 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, vec3 *result )
{
	if( value.type() == typeid( vec3 ) ) {
		*result = any_cast<vec3>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 3 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->z ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, vec4 *result )
{
	if( value.type() == typeid( vec4 ) ) {
		*result = any_cast<vec4>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 4 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->z ) )
		return false;
	if( ! getValue( (*castedVector)[3], &result->w ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, dvec2 *result )
{
	if( value.type() == typeid( dvec2 ) ) {
		*result = any_cast<dvec2>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>>( &value );
	if( ! castedVector || castedVector->size() < 2 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, dvec3 *result )
{
	if( value.type() == typeid( dvec3 ) ) {
		*result = any_cast<dvec3>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 3 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->z ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, dvec4 *result )
{
	if( value.type() == typeid( dvec4 ) ) {
		*result = any_cast<dvec4>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 4 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->z ) )
		return false;
	if( ! getValue( (*castedVector)[3], &result->w ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, ivec2 *result )
{
	if( value.type() == typeid( ivec2 ) ) {
		*result = any_cast<ivec2>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>>( &value );
	if( !castedVector || castedVector->size() < 2 )
		return false;

	if( ! getValue( ( *castedVector )[0], &result->x ) )
		return false;
	if( ! getValue( ( *castedVector )[1], &result->y ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, ivec3 *result )
{
	if( value.type() == typeid( ivec3 ) ) {
		*result = any_cast<ivec3>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( !castedVector || castedVector->size() < 3 )
		return false;

	if( ! getValue( ( *castedVector )[0], &result->x ) )
		return false;
	if( ! getValue( ( *castedVector )[1], &result->y ) )
		return false;
	if( ! getValue( ( *castedVector )[2], &result->z ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, ivec4 *result )
{
	if( value.type() == typeid( ivec4 ) ) {
		*result = any_cast<ivec4>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( !castedVector || castedVector->size() < 4 )
		return false;

	if( ! getValue( ( *castedVector )[0], &result->x ) )
		return false;
	if( ! getValue( ( *castedVector )[1], &result->y ) )
		return false;
	if( ! getValue( ( *castedVector )[2], &result->z ) )
		return false;
	if( ! getValue( ( *castedVector )[3], &result->w ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, ci::quat *result )
{
	if( value.type() == typeid( quat ) ) {
		*result = any_cast<quat>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( !castedVector || castedVector->size() < 4 )
		return false;

	if( ! getValue( ( *castedVector )[0], &result->x ) )
		return false;
	if( ! getValue( ( *castedVector )[1], &result->y ) )
		return false;
	if( ! getValue( ( *castedVector )[2], &result->z ) )
		return false;
	if( ! getValue( ( *castedVector )[3], &result->w ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, Rectf *result )
{
	if( value.type() == typeid( Rectf ) ) {
		*result = any_cast<Rectf>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 4 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->x1 ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->y1 ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->x2 ) )
		return false;
	if( ! getValue( (*castedVector)[3], &result->y2 ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, Color *result )
{
	if( value.type() == typeid( Color ) ) {
		*result = any_cast<Color>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 3 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->r ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->g ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->b ) )
		return false;

	return true;
}

bool getValue( const Info::Value &value, ColorA *result )
{
	if( value.type() == typeid( ColorA ) ) {
		*result = any_cast<ColorA>( value );
		return true;
	}

	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( ! castedVector || castedVector->size() < 3 )
		return false;

	if( ! getValue( (*castedVector)[0], &result->r ) )
		return false;
	if( ! getValue( (*castedVector)[1], &result->g ) )
		return false;
	if( ! getValue( (*castedVector)[2], &result->b ) )
		return false;

	// alpha channel is optional
	if( castedVector->size() >= 4 ) {
		if( ! getValue( (*castedVector)[3], &result->a ) )
			return false;
	}
	else {
		result->a = 1.0;
	}

	return true;
}

bool getValue( const Info::Value &value, ci::fs::path *result )
{
	if( value.type() == typeid( ci::fs::path ) ) {
		*result = any_cast<ci::fs::path>( value );
		return true;
	}

	const auto castedString = any_cast<std::string>( &value );
	if( castedString ) {
		*result = *castedString;
		return true;
	}

	return false;
}

bool getValue( const Info::Value &value, vector<Info::Value> *result )
{
	const auto castedVector = any_cast<std::vector<Info::Value>> ( &value );
	if( castedVector ) {
		*result = *castedVector;
		return true;
	}

	return false;
}

} // namespace mason::detail

// ----------------------------------------------------------------------------------------------------
// Info::Value
// ----------------------------------------------------------------------------------------------------

const Info::Value& Info::Value::operator[]( const std::string &key ) const
{ 
	const auto &dict = any_cast<const Info &>( *this );
	return dict.getStrict<Info::Value>( key );
}

Info::Value& Info::Value::operator[]( const std::string &key )
{
	// check if this is a Info. If yes, typecast to get the dictionary, then value from key
	if( type() == typeid( Info ) ) {
		auto &dict = any_cast<Info &>( *this );
		return dict[key];
	}
	else {
		// if not, then blow away current contents with an empty Info,
		// add an element for the key and return the value.		
		//emplace( Info ); // TODO: want to use this, need the C++17 version
		*this = Info();
		return (*this)[key];
	}
}

// ----------------------------------------------------------------------------------------------------
// Info <-> JSON
// ----------------------------------------------------------------------------------------------------

namespace {

// TODO: rename to toValue()
Info::Value toAny( const Json::Value &value );
Json::Value toJson( const Info::Value &a );

Info toInfo( const Json::Value &value )
{
	Info result;

	for( auto childIt = value.begin(); childIt != value.end(); ++childIt ) {
		string key = childIt.key().asString();
		auto value = toAny( *childIt );
		result.set( key, value );
	}

	return result;
}

Info::Value toAny( const Json::Value &value )
{
	if( value.isObject() ) {
		return toInfo( value );
	}
	else if( value.isArray() ) {
		vector<Info::Value> arr;
		arr.reserve( value.size() );

		for( const auto &child : value )
			arr.push_back( toAny( child ) );

		return arr;
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

Json::Value toJson( const Info &dict )
{
	Json::Value result;
	for( const auto &mp : dict.getData() ) {
		result[mp.first] = toJson( mp.second );
	}

	return result;
}

Json::Value toJson( const vector<Info::Value> &arr )
{
	Json::Value result;
	for( const auto &a : arr ) {
		result.append( toJson( a ) );
	}

	return result;
}

Json::Value toJson( const Info::Value &a )
{
	if( a.type() == typeid( Info ) ) {
		return toJson( any_cast<Info>( a ) );
	}
//	else if( a.type() == typeid( vector<Info> ) ) {
//		// TODO NEXT: implement this either with for loop over vector or another toJson overload()
////		return toJson( any_cast<vector<Info::Value>>( a ) );
//	}
	else if( a.type() == typeid( vector<Info::Value> ) ) {
		return toJson( any_cast<vector<Info::Value>>( a ) );
	}
	else if( a.type() == typeid( int ) ) {
		return Json::Value( any_cast<int>( a ) );
	}
	else if( a.type() == typeid( bool ) ) {
		return Json::Value( any_cast<bool>( a ) );
	}
	else if( a.type() == typeid( float ) ) {
		return Json::Value( any_cast<float>( a ) );
	}
	else if( a.type() == typeid( double ) ) {
		return Json::Value( any_cast<double>( a ) );
	}
	else if( a.type() == typeid( string ) ) {
		return Json::Value( any_cast<string>( a ) );
	}
	else if( a.type() == typeid( const char* ) ) {
		return Json::Value( any_cast<const char *>( a ) );
	}
	else if( a.type() == typeid( vec2 ) ) {
		auto v = any_cast<vec2>( a );
		Json::Value result;
		result.append( v[0] );
		result.append( v[1] );
		return result;
	}
	else if( a.type() == typeid( vec3 ) ) {
		auto v = any_cast<vec3>( a );
		Json::Value result;
		result.append( v[0] );
		result.append( v[1] );
		result.append( v[2] );
		return result;
	}
	else if( a.type() == typeid( vec4 ) ) {
		auto v = any_cast<vec4>( a );
		Json::Value result;
		result.append( v[0] );
		result.append( v[1] );
		result.append( v[2] );
		result.append( v[3] );
		return result;
	}
	else if( a.type() == typeid( glm::quat ) ) {
		auto v = any_cast<glm::quat>( a );
		Json::Value result;
		result.append( v[0] );
		result.append( v[1] );
		result.append( v[2] );
		result.append( v[3] );
		return result;
	}
	else if( a.type() == typeid( Color ) ) {
		auto c = any_cast<Color>( a );
		Json::Value result;
		result.append( c.r );
		result.append( c.g );
		result.append( c.b );
		return result;
	}
	else if( a.type() == typeid( ColorA ) ) {
		auto c = any_cast<ColorA>( a );
		Json::Value result;
		result.append( c.r );
		result.append( c.g );
		result.append( c.b );
		result.append( c.a );
		return result;
	}
	// FIXME: Json::Value::null doesn't seem to be exported with jsoncpp
	//else if( a.type() == typeid( nullptr ) ) {
	//	return Json::Value::null;
	//}
	else {
		// Unknown type, write out as a string with type info.
		// TODO: consider what best to do here. Could log an error, or provide a callback handler.
		string typeAsString = System::demangleTypeName( a.type().name() );
		//CI_ASSERT_NOT_REACHABLE();
		return Json::Value( "type '" + typeAsString + "'" );
	}
}

} // anonymous namespace

// static
template<>
MA_API Info Info::convert<Json::Value>( const Json::Value &data )
{
	Info result =	toInfo( data );
	return result;
}

// static
template<>
MA_API Info Info::convert<Json::Value>( const DataSourceRef &dataSource )
{
	string dataString = loadString( dataSource );

	Json::Reader reader;
	Json::Value data;

	bool success = reader.parse( dataString, data );
	if( ! success ) {
		string errorMessage = reader.getFormattedErrorMessages();
		throw InfoExc( "Json::Reader failed to parse source, error message: " + errorMessage );
	}

	return convert( data );
}

template<>
MA_API Json::Value Info::convert<Json::Value>() const
{
	Json::Value result = toJson( *this );	
	return result;
}

// TODO: implement this natively. using jsoncpp for now
std::ostream& operator<<( std::ostream &os, const Info &rhs )
{
	auto json = rhs.convert<Json::Value>();
	os << json;
	return os;
}

} // namespace mason
