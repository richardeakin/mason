
// Copyright 2011-2018 Molecular Matters e.U., all rights reserved. All registered trademarks are the properties of their respective owners.
// The contents of this file are strictly confidential, and are covered under copyright and trademark laws by Molecular Matters e.U.
// This file is part of the Live++ API, and is subject to the terms and conditions and/or a license agreement in their latest versions.

#pragma once

/******************************************************************************/
/* HOOKS                                                                      */
/******************************************************************************/

// concatenates two preprocessor tokens, even when the tokens themselves are macros
#define LPP_CONCATENATE_HELPER_HELPER(_a, _b)		_a##_b
#define LPP_CONCATENATE_HELPER(_a, _b)				LPP_CONCATENATE_HELPER_HELPER(_a, _b)
#define LPP_CONCATENATE(_a, _b)						LPP_CONCATENATE_HELPER(_a, _b)

// generates a unique identifier inside a translation unit
#define LPP_IDENTIFIER(_identifier)					LPP_CONCATENATE(_identifier, __LINE__)

// custom section names for hooks
#define LPP_PREPATCH_SECTION			".lpp_prepatch_hooks"
#define LPP_POSTPATCH_SECTION			".lpp_postpatch_hooks"
#define LPP_COMPILE_SUCCESS_SECTION		".lpp_compile_success_hooks"
#define LPP_COMPILE_ERROR_SECTION		".lpp_compile_error_hooks"

// register a pre-patch hook in a custom section
#define LPP_PREPATCH_HOOK(_function)																							\
	__pragma(section(LPP_PREPATCH_SECTION, read))																				\
	__declspec(allocate(LPP_PREPATCH_SECTION)) extern void (*LPP_IDENTIFIER(lpp_prepatch_hook_function))(void) = &_function

// register a post-patch hook in a custom section
#define LPP_POSTPATCH_HOOK(_function)																							\
	__pragma(section(LPP_POSTPATCH_SECTION, read))																				\
	__declspec(allocate(LPP_POSTPATCH_SECTION)) extern void (*LPP_IDENTIFIER(lpp_postpatch_hook_function))(void) = &_function

// register a compile success hook in a custom section
#define LPP_COMPILE_SUCCESS_HOOK(_function)																									\
	__pragma(section(LPP_COMPILE_SUCCESS_SECTION, read))																					\
	__declspec(allocate(LPP_COMPILE_SUCCESS_SECTION)) extern void (*LPP_IDENTIFIER(lpp_compile_success_hook_function))(void) = &_function

// register a compile error hook in a custom section
#define LPP_COMPILE_ERROR_HOOK(_function)																								\
	__pragma(section(LPP_COMPILE_ERROR_SECTION, read))																					\
	__declspec(allocate(LPP_COMPILE_ERROR_SECTION)) extern void (*LPP_IDENTIFIER(lpp_compile_error_hook_function))(void) = &_function



/******************************************************************************/
/* API                                                                        */
/******************************************************************************/

// version string
#define LPP_VERSION "1.2.3"

// macros to temporarily enable/disable optimizations
#define LPP_ENABLE_OPTIMIZATIONS		__pragma(optimize("", on))
#define LPP_DISABLE_OPTIMIZATIONS		__pragma(optimize("", off))

// linker pseudo-variable representing the DOS header of the module we're being compiled into:
// Raymond Chen: https://blogs.msdn.microsoft.com/oldnewthing/20041025-00/?p=37483
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

#ifdef __cplusplus
#	define LPP_NS_BEGIN			namespace lpp {
#	define LPP_NS_END			}
#	define LPP_API				inline
#else
#	define LPP_NS_BEGIN
#	define LPP_NS_END
#	define LPP_API				static inline
#endif

// helper macro to call a function in a DLL with an arbitrary signature without a compiler warning
#define LPP_CALL(_module, _functionName, _return, _args)		((_return (__cdecl*)(_args))((uintptr_t)GetProcAddress(_module, _functionName)))


LPP_NS_BEGIN

// retrieves the version number of the DLL.
LPP_API const char* lppGetVersion(HMODULE livePP)
{
	return LPP_CALL(livePP, "LppGetVersion", const char*, void)();
}

// checks whether the API and DLL versions match.
// returns 1 on success, 0 on failure.
LPP_API int lppCheckVersion(HMODULE livePP)
{
	return LPP_CALL(livePP, "LppCheckVersion", int, const char* const)(LPP_VERSION);
}


// registers a Live++ process group.
LPP_API void lppRegisterProcessGroup(HMODULE livePP, const char* const groupName)
{
	LPP_CALL(livePP, "LppRegisterProcessGroup", void, const char* const)(groupName);
}


// calls the synchronization point.
LPP_API void lppSyncPoint(HMODULE livePP)
{
	LPP_CALL(livePP, "LppSyncPoint", void, void)();
}


// waits until the operation identified by the given token is finished.
LPP_API void lppWaitForToken(HMODULE livePP, void* token)
{
	LPP_CALL(livePP, "LppWaitForToken", void, void*)(token);
}


// triggers a recompile.
LPP_API void lppTriggerRecompile(HMODULE livePP)
{
	LPP_CALL(livePP, "LppTriggerRecompile", void, void)();
}


// asynchronously enables Live++ for just the given .exe or .dll, but not its import modules.
// returns a token that can be waited upon using lppWaitForToken().
LPP_API void* lppEnableModuleAsync(HMODULE livePP, const wchar_t* const nameOfExeOrDll)
{
	return LPP_CALL(livePP, "LppEnableModule", void*, const wchar_t* const)(nameOfExeOrDll);
}

// asynchronously enables Live++ for the given .exe or .dll and all its import modules.
// returns a token that can be waited upon using lppWaitForToken().
LPP_API void* lppEnableAllModulesAsync(HMODULE livePP, const wchar_t* const nameOfExeOrDll)
{
	return LPP_CALL(livePP, "LppEnableAllModules", void*, const wchar_t* const)(nameOfExeOrDll);
}

// asynchronously enables Live++ for just the .exe or .dll this function is being called from, but not its import modules.
// returns a token that can be waited upon using lppWaitForToken().
LPP_API void* lppEnableCallingModuleAsync(HMODULE livePP)
{
	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW((HMODULE)(&__ImageBase), path, MAX_PATH);
	return lppEnableModuleAsync(livePP, path);
}

// asynchronously enables Live++ for the .exe or .dll this function is being called from and all its import modules.
// returns a token that can be waited upon using lppWaitForToken().
LPP_API void* lppEnableAllCallingModulesAsync(HMODULE livePP)
{
	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW((HMODULE)(&__ImageBase), path, MAX_PATH);
	return lppEnableAllModulesAsync(livePP, path);
}


// synchronously enables Live++ for just the given .exe or .dll, but not its import modules.
LPP_API void lppEnableModuleSync(HMODULE livePP, const wchar_t* const nameOfExeOrDll)
{
	void* token = lppEnableModuleAsync(livePP, nameOfExeOrDll);
	lppWaitForToken(livePP, token);
}

// synchronously enables Live++ for the given .exe or .dll and all its import modules.
LPP_API void lppEnableAllModulesSync(HMODULE livePP, const wchar_t* const nameOfExeOrDll)
{
	void* token = lppEnableAllModulesAsync(livePP, nameOfExeOrDll);
	lppWaitForToken(livePP, token);
}

// synchronously enables Live++ for just the .exe or .dll this function is being called from, but not its import modules.
LPP_API void lppEnableCallingModuleSync(HMODULE livePP)
{
	void* token = lppEnableCallingModuleAsync(livePP);
	lppWaitForToken(livePP, token);
}

// synchronously enables Live++ for the .exe or .dll this function is being called from and all its import modules.
LPP_API void lppEnableAllCallingModulesSync(HMODULE livePP)
{
	void* token = lppEnableAllCallingModulesAsync(livePP);
	lppWaitForToken(livePP, token);
}


// loads the correct 32-bit/64-bit DLL (based on architecture), checks if the versions match, and registers a Live++ process group.
LPP_API HMODULE lppLoadAndRegister(const wchar_t* pathWithoutTrailingSlash, const char* const groupName)
{
	wchar_t path[1024] = { 0 };

	// we deliberately do not use memcpy or strcpy here, as that could force more #includes in the user's code
	unsigned int index = 0u;
	while (*pathWithoutTrailingSlash != L'\0')
	{
		if (index >= 1024u)
		{
			// no space left in buffer
			return NULL;
		}

		path[index++] = *pathWithoutTrailingSlash++;
	}

#ifdef _WIN64
	const wchar_t* dllName = L"/x64/LPP_x64.dll";
#else
	const wchar_t* dllName = L"/x86/LPP_x86.dll";
#endif

	while (*dllName != L'\0')
	{
		if (index >= 1024u)
		{
			// no space left in buffer
			return NULL;
		}

		path[index++] = *dllName++;
	}

	path[index++] = L'\0';

	HMODULE livePP = LoadLibraryW(path);
	if (livePP)
	{
		if (!lppCheckVersion(livePP))
		{
			// version mismatch detected
			FreeLibrary(livePP);
			return NULL;
		}

		lppRegisterProcessGroup(livePP, groupName);
	}

	return livePP;
}

LPP_NS_END

#undef LPP_CALL
