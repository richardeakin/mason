
// Copyright 2011-2018 Molecular Matters e.U., all rights reserved. All registered trademarks are the properties of their respective owners.
// The contents of this file are strictly confidential, and are covered under copyright and trademark laws by Molecular Matters e.U.
// This file is part of the Live++ API, and is subject to the terms and conditions and/or a license agreement in their latest versions.

/******************************************************************************/
/* MACRO HELPERS															  */
/******************************************************************************/

// concatenates two preprocessor tokens, even when the tokens themselves are macros
#define LPP_STATIC_RT_CONCATENATE_HELPER_HELPER(_a, _b)		_a##_b
#define LPP_STATIC_RT_CONCATENATE_HELPER(_a, _b)			LPP_STATIC_RT_CONCATENATE_HELPER_HELPER(_a, _b)
#define LPP_STATIC_RT_CONCATENATE(_a, _b)					LPP_STATIC_RT_CONCATENATE_HELPER(_a, _b)

// stringizes a preprocessor token, even when the token itself is a macro
#define LPP_STATIC_RT_STRINGIZE_HELPER(_a)					#_a
#define LPP_STATIC_RT_STRINGIZE(_a)							LPP_STATIC_RT_STRINGIZE_HELPER(_a)



/******************************************************************************/
/* SETUP																	  */
/******************************************************************************/

#if _MSC_VER <= 1600
#	define LPP_STATIC_RT_VERSION VS2010_MT
#elif _MSC_VER == 1700
#	define LPP_STATIC_RT_VERSION VS2012_MT
#elif _MSC_VER == 1800
#	define LPP_STATIC_RT_VERSION VS2013_MT
#elif _MSC_VER == 1900
#	define LPP_STATIC_RT_VERSION VS2015_MT
#elif _MSC_VER > 1910
#	define LPP_STATIC_RT_VERSION VS2017_MT
#endif

#ifdef _DEBUG
#	if _DEBUG == 1
#		define LPP_STATIC_RT_TYPE_SUFFIX d_
#	else
#		define LPP_STATIC_RT_TYPE_SUFFIX _
#	endif
#else
#	define LPP_STATIC_RT_TYPE_SUFFIX _
#endif

#ifdef _WIN64
#	if _WIN64 == 1
#		define LPP_STATIC_RT_ARCH_SUFFIX x64.h
#	else
#		define LPP_STATIC_RT_ARCH_SUFFIX x86.h
#	endif
#else
#	define LPP_STATIC_RT_ARCH_SUFFIX x86.h
#endif


// forces a symbol to be linked
#define LPP_STATIC_RT_FORCE_LINK(_symbol)			__pragma(comment(linker, "/INCLUDE:"##_symbol))

// generate the name of the file to be included, based on Visual Studio version, architecture and build type
#define LPP_STATIC_RT_INCLUDE_FILE LPP_STATIC_RT_CONCATENATE(LPP_STATIC_RT_CONCATENATE(LPP_STATIC_RT_CONCATENATE(LPP_, LPP_STATIC_RT_VERSION), LPP_STATIC_RT_TYPE_SUFFIX), LPP_STATIC_RT_ARCH_SUFFIX)

// include the file that pulls in symbols from all .obj files contained in the static runtime library that expose at least one dynamic initializer or atexit destructor
#include LPP_STATIC_RT_STRINGIZE(LPP_STATIC_RT_INCLUDE_FILE)



/******************************************************************************/
/* CLEANUP																	  */
/******************************************************************************/

#undef LPP_STATIC_RT_CONCATENATE_HELPER_HELPER
#undef LPP_STATIC_RT_CONCATENATE_HELPER
#undef LPP_STATIC_RT_CONCATENATE
#undef LPP_STATIC_RT_STRINGIZE_HELPER
#undef LPP_STATIC_RT_STRINGIZE

#undef LPP_STATIC_RT_VERSION
#undef LPP_STATIC_RT_TYPE_SUFFIX
#undef LPP_STATIC_RT_ARCH_SUFFIX

#undef LPP_STATIC_RT_FORCE_LINK
#undef LPP_STATIC_RT_INCLUDE_FILE
