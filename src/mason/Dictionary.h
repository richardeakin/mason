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
#include "cinder/Color.h"
#include "cinder/Rect.h"

#include <memory>
#include <map>

// TODO: switch to std::any on toolsets that supply it
#include <boost/any.hpp>

#include "mason/Mason.h"

namespace mason {

using DictionaryRef = std::shared_ptr<class Dictionary>;


//! Object for storing dynamic data in a hierarchical form, key = string, value = any.
class MA_API Dictionary {
  public:

	using AnyT = boost::any;
	class Value : public AnyT {
	  public:

		Value() BOOST_NOEXCEPT
			: AnyT()
		{}

		template<typename T>
		Value( const T &value )
			: AnyT( static_cast<const AnyT &>( value ) )
		{}

		Value( const Value &other )
			: AnyT( static_cast<const AnyT &>( other ) )
		{}

		Value & operator=( const Value& rhs )
		{
			AnyT::operator=( static_cast<const AnyT &>( rhs ) );
			return *this;
		}

		// move assignement
		Value & operator=( Value&& rhs ) BOOST_NOEXCEPT
		{
			AnyT::operator=( static_cast<AnyT &&>( rhs ) );
			return *this;
		}

		// Perfect forwarding of Value
		template <class T>
		Value & operator=( T&& rhs )
		{
			AnyT::operator=( static_cast<AnyT &&>( rhs ) );
			return *this;
		}

		//! Conversion operator for anything supported by detail::getValue()
		template <typename T>
		operator T() const
		{
			T result;
			if( ! detail::getValue( *this, &result ) ) {
				throw DictionaryBadTypeExc( *this, typeid( T ) );
			}

			return result;
		}

		const Dictionary::Value& operator[]( const std::string &key ) const;
	};

	//! Constructs an empty Dictionary on the stack
	Dictionary();
	//! Creates an empty Dictionary and returns it in a shared_ptr
	static DictionaryRef	create()	{ return std::make_shared<Dictionary>(); }

	template<typename DataT>
	static Dictionary		convert( const DataT &data );

	template<typename DataT>
	static Dictionary		convert( const ci::DataSourceRef &dataSource );

	template<typename DataT>
	static Dictionary		convert( const ci::fs::path &filePath );

	//! Converts and returns this Dictionary as type DataT.
	template<typename DataT>
	DataT		convert() const;

	// TODO: possible to reuse above converts but store in shared_ptr?
//	template<typename DataT, typename... Args>
//	static DictionaryRef	convertShared( Args... )

	template<typename T>
	void		set( const std::string &key, const T &value );

	//! Returns a copy of the value associated with T. Some type conversions are allowed (like double -> float), hence why a copy is necessary.
	//! A DictionaryExc is thrown if the key doesn't exist or it can't be converted to type T.
	template<typename T>
	T	get( const std::string &key ) const;
	//! Same as above but will return \a defaultValue if the key doesn't exist.
	//! A DictionaryExc is thrown if the key exists but can't be converted to type T.
	template<typename T>
	T	get( const std::string &key, const T &defaultValue ) const;
	//! Returns a reference to the value associated with T. The typeid must match exactly.
	//! TODO: consider renaming to getRef(), which matches some other impls
	template<typename T>
	const T&	getStrict( const std::string &key ) const;
	template<>
	const Dictionary::Value& getStrict<Dictionary::Value>( const std::string &key ) const;
	//! Returns a reference to the value associated with T. The typeid must match exactly. Returns \a defaultValue if the key doesn't exist.
	template<typename T>
	const T&	getStrict( const std::string &key, const T &defaultValue ) const;

	const std::type_info&   getType( const std::string &key ) const;

	bool contains( const std::string &key ) const;

	//! Recursively copy the values of other into this Dictionary.
	void merge( const Dictionary &other );

	const std::map<std::string, Value>&	getData() const	{ return mData; }

	size_t getSize() const  { return mData.size(); }
	std::vector<std::string>	getAllKeys() const;

	bool isEmpty() const		{ return mData.empty(); }

	//! operator enabling lookup by key. Enables syntax like `float x = dict["x"];`.
	const Dictionary::Value& operator[]( const std::string &key ) const;
	//! operator enabling lookup by key. Enables syntax like `dict["x"] = 1.0f`.
	Dictionary::Value& operator[]( const std::string &key );
	//! Convert this Dictionary to a string representation.
	std::string	toString() const;


	friend MA_API std::ostream& operator<<( std::ostream &os, const Dictionary &rhs );

  private:

	std::map<std::string, Value>	mData;
};

class MA_API DictionaryExc : public ci::Exception {
  public:
	DictionaryExc( const std::string &description )
			: Exception( description )
	{ }
};

class MA_API DictionaryBadTypeExc : public DictionaryExc {
  public:
	DictionaryBadTypeExc( const Dictionary::Value &value, const std::type_info &typeInfo )
		: DictionaryExc( "" )
	{
		std::string descr = "Failed to convert value during type conversion, from native type '";
		descr += ci::System::demangleTypeName( value.type().name() ) + "' to requested type '";
		descr += ci::System::demangleTypeName( typeInfo.name() ) + "'.";
		setDescription( descr );
	}

	DictionaryBadTypeExc( const std::string &key, const Dictionary::Value &value, const std::type_info &typeInfo )
		: DictionaryExc( "" )
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
bool getValue( const Dictionary::Value &value, T *result )
{
	if( value.type() != typeid( T ) ) {
		return false;
	}

	*result = boost::any_cast<T>( value );
	return true;
}

template<typename T>
bool getValue( const Dictionary::Value &value, std::vector<T> *result )
{
	const auto castedVector = boost::any_cast<std::vector<Dictionary::Value>> ( &value );
	if( ! castedVector )
		return false;
	
	result->resize( castedVector->size() );
	for( size_t i = 0; i < result->size(); i++ ) {
		getValue( (*castedVector)[i], &( (*result)[i] ) );
	}

	return true;
}

bool MA_API getValue( const Dictionary::Value &value, float *result );
bool MA_API getValue( const Dictionary::Value &value, double *result );
bool MA_API getValue( const Dictionary::Value &value, size_t *result );
bool MA_API getValue( const Dictionary::Value &value, int32_t *result );
bool MA_API getValue( const Dictionary::Value &value, uint32_t *result );
bool MA_API getValue( const Dictionary::Value &value, ci::vec2 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::vec3 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::vec4 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::dvec2 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::dvec3 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::dvec4 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::ivec2 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::ivec3 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::ivec4 *result );
bool MA_API getValue( const Dictionary::Value &value, ci::Rectf *result );
bool MA_API getValue( const Dictionary::Value &value, ci::Color *result );
bool MA_API getValue( const Dictionary::Value &value, ci::ColorA *result );
bool MA_API getValue( const Dictionary::Value &value, ci::fs::path *result );
bool MA_API getValue( const Dictionary::Value &value, std::vector<Dictionary::Value> *result );

} // namespace mason::detail

template<typename T>
void Dictionary::set( const std::string &key, const T &value )
{
	mData[key] = value;
}

template<typename T>
T Dictionary::get( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw DictionaryExc( "no key named '" + key + "'" );
	}

	T result;
	const auto &value = it->second;

	if( ! detail::getValue( value, &result ) ) {
		throw DictionaryBadTypeExc( key, value, typeid( T ) );
	}

	return result;
}

template<typename T>
T Dictionary::get( const std::string &key, const T &defaultValue ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		return defaultValue;
	}

	T result;
	const auto &value = it->second;

	if( ! detail::getValue( value, &result ) ) {
		throw DictionaryBadTypeExc( key, value, typeid( T ) );
	}

	return result;
}

template<typename T>
const T& Dictionary::getStrict( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw DictionaryExc( "no key named '" + key + "'" );
	}

	const auto &value = it->second;

	if( value.type() != typeid( T ) ) {
		throw DictionaryBadTypeExc( key, value, typeid( T ) );
	}

	return boost::any_cast<const T&>( value );
}

template<typename T>
const T& Dictionary::getStrict( const std::string &key, const T &defaultValue ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		return defaultValue;
	}

	const auto &value = it->second;

	if( value.type() != typeid( T ) ) {
		throw DictionaryBadTypeExc( key, value, typeid( T ) );
	}

	return boost::any_cast<const T&>( value );
}

// ----------------------------------------------------
// TODO: move these to cpp

template<>
const Dictionary::Value& Dictionary::getStrict<Dictionary::Value>( const std::string &key ) const
{
	auto it = mData.find( key );
	if( it == mData.end() ) {
		throw DictionaryExc( "no key named '" + key + "'" );
	}

	return it->second;
}

} // namespace mason
