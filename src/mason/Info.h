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

#include "cinder/Filesystem.h"
#include "cinder/DataSource.h"
#include "cinder/Exception.h"
#include "cinder/System.h"
#include "cinder/Vector.h"
#include "cinder/Quaternion.h"
#include "cinder/Color.h"
#include "cinder/Rect.h"

#include <memory>
#include <map>

#if defined( CINDER_UWP ) || ( defined( _MSC_VER ) && ( _MSC_VER >= 1923 ) )
#define MA_HAVE_STD_ANY
#include <any>
#else
#include <boost/any.hpp>
#endif

#include "mason/Mason.h"

namespace mason {

using InfoRef = std::shared_ptr<class Info>;

#if defined( MA_HAVE_STD_ANY )
using std::any;
using std::any_cast;
#else
using boost::any;
using boost::any_cast;
#endif


//! Object for storing dynamic data in a hierarchical form, key = string, value = any.
class MA_API Info {
  public:

	class Value : public any {
	  public:

		Value() noexcept
			: any()
		{}

		template<typename T>
		Value( const T &value )
			: any( static_cast<const any &>( value ) )
		{}

		Value( const Value &other )
			: any( static_cast<const any &>( other ) )
		{}

		Value & operator=( const Value& rhs )
		{
			any::operator=( static_cast<const any &>( rhs ) );
			return *this;
		}

		// move assignement
		Value & operator=( Value&& rhs ) noexcept
		{
			any::operator=( static_cast<any &&>( rhs ) );
			return *this;
		}

		// Perfect forwarding of Value
		template <class T>
		Value & operator=( T&& rhs )
		{
			any::operator=( static_cast<any &&>( rhs ) );
			return *this;
		}

		// FIXME: tried to get this overload working to fix vector<int> problem, not yet working
		template <class T>
		Value & operator=( std::vector<T>&& rhs )
		{
			any::operator=( static_cast<any &&>( rhs ) );
			return *this;
		}

		//! Conversion operator for anything supported by detail::getValue()
		template <typename T>
		operator T() const;

		const Info::Value& operator[]( const std::string &key ) const;
		Info::Value& operator[]( const std::string &key );
	};

	using Container = std::map<std::string, Value>;
	using Iterator = Container::iterator;
	using ConstIterator = Container::const_iterator;


	//! Constructs an empty Info on the stack
	Info();
	//! Creates an empty Info and returns it in a shared_ptr
	static InfoRef	create()	{ return std::make_shared<Info>(); }

	template<typename DataT>
	static Info		convert( const DataT &data );

	template<typename DataT>
	static Info		convert( const ci::DataSourceRef &dataSource );

	template<typename DataT>
	static Info		convert( const ci::fs::path &filePath );

	//! Converts and returns this Info as type DataT.
	template<typename DataT>
	DataT		convert() const;

	// TODO: possible to reuse above converts but store in shared_ptr?
//	template<typename DataT, typename... Args>
//	static InfoRef	convertShared( Args... )

	template<typename T>
	void		set( const std::string &key, const T &value );
	template<typename T>
	void set( const std::string &key, const std::vector<T> &value );

	//! Returns a copy of the value associated with T. Some type conversions are allowed (like double -> float), hence why a copy is necessary.
	//! A InfoExc is thrown if the key doesn't exist or it can't be converted to type T.
	template<typename T>
	T	get( const std::string &key ) const;
	//! Same as above but will return \a defaultValue if the key doesn't exist.
	//! A InfoExc is thrown if the key exists but can't be converted to type T.
	template<typename T>
	T	get( const std::string &key, const T &defaultValue ) const;
	//! Returns a reference to the value associated with T. The typeid must match exactly.
	//! TODO: consider renaming to getRef(), which matches some other impls
	template<typename T>
	const T&	getStrict( const std::string &key ) const;
	template<>
	const Value& getStrict<Info::Value>( const std::string &key ) const;
	//! Returns a reference to the value associated with T. The typeid must match exactly. Returns \a defaultValue if the key doesn't exist.
	template<typename T>
	const T&	getStrict( const std::string &key, const T &defaultValue ) const;

	const std::type_info&   getType( const std::string &key ) const;

	bool contains( const std::string &key ) const;

	//! Recursively copy the values of other into this Info.
	void merge( const Info &other );

	Iterator			begin()			{ return mData.begin(); }
	const ConstIterator	begin() const	{ return mData.begin(); }
	Iterator			end()			{ return mData.end(); }
	const ConstIterator	end() const		{ return mData.end(); }
	//! Returns the underlining data container.
	const Container&	getData() const	{ return mData; }

	size_t getSize() const  { return mData.size(); }
	std::vector<std::string>	getAllKeys() const;

	bool isEmpty() const		{ return mData.empty(); }

	//! operator enabling lookup by key. Enables syntax like `float x = dict["x"];`.
	const Info::Value& operator[]( const std::string &key ) const;
	//! operator enabling lookup by key. Enables syntax like `dict["x"] = 1.0f`.
	Info::Value& operator[]( const std::string &key );
	//! Convert this Info to a string representation.
	std::string	toString() const;


	friend MA_API std::ostream& operator<<( std::ostream &os, const Info &rhs );

  private:

	std::map<std::string, Value>	mData;
};

class MA_API InfoExc : public ci::Exception {
  public:
	InfoExc( const std::string &description )
			: Exception( description )
	{ }
};

class MA_API InfoBadTypeExc : public InfoExc {
  public:
	InfoBadTypeExc( const Info::Value &value, const std::type_info &typeInfo )
		: InfoExc( "" )
	{
		std::string descr = "Failed to convert value during type conversion, from native type '";
		descr += ci::System::demangleTypeName( value.type().name() ) + "' to requested type '";
		descr += ci::System::demangleTypeName( typeInfo.name() ) + "'.";
		setDescription( descr );
	}

	InfoBadTypeExc( const std::string &key, const Info::Value &value, const std::type_info &typeInfo )
		: InfoExc( "" )
	{
		std::string descr = "Failed to convert value for key '" + key + "', from native type '";
		descr += ci::System::demangleTypeName( value.type().name() ) + "' to requested type '";
		descr += ci::System::demangleTypeName( typeInfo.name() ) + "'.";
		setDescription( descr );
	}
};

// ----------------------------------------------------------------------------------------------------
// Implementation
// ----------------------------------------------------------------------------------------------------
// Note detail::getValue<T> - if one doesn't exist for a user's type, you can implement a specialized
// function for that type.

namespace detail {

template<typename T>
bool getValue( const Info::Value &value, T *result )
{
	if( value.type() != typeid( T ) ) {
		return false;
	}

	*result = any_cast<T>( value );
	return true;
}

template<typename T>
bool getValue( const Info::Value &value, std::vector<T> *result )
{
	const auto castedVector = any_cast<std::vector<Info::Value>>( &value );
	if( castedVector ) {
		result->resize( castedVector->size() );
		for( size_t i = 0; i < result->size(); i++ ) {
			getValue( (*castedVector)[i], &( (*result)[i] ) );
		}

		return true;
	}

	// Convert null to an empty vector
	if( value.type() == typeid( nullptr ) ) {
		result->clear();
		return true;
	}

	
	return false;
}

bool MA_API getValue( const Info::Value &value, float *result );
bool MA_API getValue( const Info::Value &value, double *result );
bool MA_API getValue( const Info::Value &value, size_t *result );
bool MA_API getValue( const Info::Value &value, int32_t *result );
bool MA_API getValue( const Info::Value &value, uint32_t *result );
bool MA_API getValue( const Info::Value &value, ci::vec2 *result );
bool MA_API getValue( const Info::Value &value, ci::vec3 *result );
bool MA_API getValue( const Info::Value &value, ci::vec4 *result );
bool MA_API getValue( const Info::Value &value, ci::dvec2 *result );
bool MA_API getValue( const Info::Value &value, ci::dvec3 *result );
bool MA_API getValue( const Info::Value &value, ci::dvec4 *result );
bool MA_API getValue( const Info::Value &value, ci::ivec2 *result );
bool MA_API getValue( const Info::Value &value, ci::ivec3 *result );
bool MA_API getValue( const Info::Value &value, ci::ivec4 *result );
bool MA_API getValue( const Info::Value &value, ci::quat *result );
bool MA_API getValue( const Info::Value &value, ci::Rectf *result );
bool MA_API getValue( const Info::Value &value, ci::Color *result );
bool MA_API getValue( const Info::Value &value, ci::ColorA *result );
bool MA_API getValue( const Info::Value &value, ci::fs::path *result );
bool MA_API getValue( const Info::Value &value, std::vector<Info::Value> *result );

} // namespace mason::detail

template<typename T>
void Info::set( const std::string &key, const T &value )
{
	mData[key] = value;
}

template<typename T>
void Info::set( const std::string &key, const std::vector<T> &value )
{
	// Force internal storage to be of type vector<Info::Value> to simplify output converters
	if( typeid( value ) == typeid( vector<Info::Value> ) ) {
		mData[key] = value;
	}
	else {
		std::vector<Info::Value> valueCoerced;
		valueCoerced.reserve( value.size() );
		for( const auto &x : value ) {
			valueCoerced.push_back( x );
		}
		mData[key] = valueCoerced;
	}
}

template<typename T>
T Info::get( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw InfoExc( "no key named '" + key + "'" );
	}

	T result;
	const auto &value = it->second;

	if( ! detail::getValue( value, &result ) ) {
		throw InfoBadTypeExc( key, value, typeid( T ) );
	}

	return result;
}

template<typename T>
T Info::get( const std::string &key, const T &defaultValue ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		return defaultValue;
	}

	T result;
	const auto &value = it->second;

	if( ! detail::getValue( value, &result ) ) {
		throw InfoBadTypeExc( key, value, typeid( T ) );
	}

	return result;
}

template<typename T>
const T& Info::getStrict( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw InfoExc( "no key named '" + key + "'" );
	}

	const auto &value = it->second;

	if( value.type() != typeid( T ) ) {
		throw InfoBadTypeExc( key, value, typeid( T ) );
	}

	return any_cast<const T&>( value );
}

template<typename T>
const T& Info::getStrict( const std::string &key, const T &defaultValue ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		return defaultValue;
	}

	const auto &value = it->second;

	if( value.type() != typeid( T ) ) {
		throw InfoBadTypeExc( key, value, typeid( T ) );
	}

	return any_cast<const T&>( value );
}

template <typename T>
Info::Value::operator T() const
{
	T result;
	if( ! detail::getValue( *this, &result ) ) {
		throw InfoBadTypeExc( *this, typeid( T ) );
	}

	return result;
}

} // namespace mason
