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

namespace mason {

class VarOwner {
  public:
	// TODO: try making these private and VarBase friend
	virtual void removeTarget( void *target ) = 0;
	virtual void cloneAndReplaceTarget( void *target, void *replacementTarget ) = 0; // TODO: rename cloneWithNewTarget - don't replace in impl
};

class VarBase {
  public:
	void setOwner( VarOwner *owner )	{ mOwner = owner; }

  protected:
	VarBase( void *voidPtr )
		: mVoidPtr( voidPtr ), mOwner( nullptr )
	{}

	VarBase( const VarBase &rhs, void *voidPtr )
		: mVoidPtr( voidPtr )
	{
		mOwner = rhs.mOwner;
		if( mOwner )
			mOwner->cloneAndReplaceTarget( rhs.mVoidPtr, mVoidPtr );
	}

	~VarBase()
	{
		if( mOwner )
			mOwner->removeTarget( mVoidPtr );
	}

	void set( const VarBase &rhs )
	{
		mOwner = rhs.mOwner;
		if( mOwner )
			mOwner->cloneAndReplaceTarget( rhs.mVoidPtr, mVoidPtr );
	}

	void*			mVoidPtr;
	VarOwner*		mOwner;
};

template<typename T>
class Var : public VarBase {
  public:
	Var()
		: VarBase( &mValue )
	{}

  	Var( T value )
		: VarBase( &mValue), mValue( value )
	{}

	// copy constructor
  	Var( const Var<T> &rhs )
		: VarBase( rhs, &mValue ), mValue( rhs.mValue )
  	{}

	// copy assignment
	Var<T>& operator=( const Var &rhs )
	{
		if( this != &rhs ) {
			set( rhs );
			mValue = rhs.mValue;
		}
		return *this;
  	}

	operator const T&() const { return mValue; }

	Var<T>& operator=( T value ) { mValue = value; return *this; }

	const T&	value() const { return mValue; }
	T&			value() { return mValue; }

	//! Short-hand for value()
	const T&	operator()() const { return mValue; }
	//! Short-hand for value()
	T&			operator()() { return mValue; }

	const T*		ptr() const { return &mValue; }
  	T*				ptr() { return &mValue; }

  protected:
	T	mValue;
};

} // namespace mason
