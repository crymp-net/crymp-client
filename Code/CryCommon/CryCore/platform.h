////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   platform.h
//  Version:     v1.00
//  Created:     11/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Platform dependend stuff.
//               Include this file instead of windows.h
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

//
// Translate some predefined macros.
//

// NDEBUG disables std asserts, etc.
// Define it automatically if not compiling with Debug libs, or with ADEBUG flag.
#if !defined(_DEBUG) && !defined(ADEBUG) && !defined(NDEBUG)
	#define NDEBUG
#endif

// We use WIN macros without _.
#if defined(_WIN32) && !defined(XENON) && !defined(LINUX32) && !defined(LINUX64) && !defined(WIN32)
	#define WIN32
#endif
#if defined(_WIN64) && !defined(WIN64)
	#define WIN64
#endif

// All windows targets built as DLLs.
#if defined(WIN32)
	#ifndef _USRDLL
		#define _USRDLL
	#endif
#else
	//#define _LIB
#endif

// Function attribute for printf/scanf-style parameters.
// This enables extended argument checking by GCC.
//
// Usage:
// Put this after the function or method declaration (not the definition!),
// between the final closing parenthesis and the semicolon.
// The first parameter indicates the 1-based index of the format string
// parameter, the second parameter indicates the 1-based index of the first
// variable parameter.  Example:
//   void foobar(int a, const char *fmt, ...) PRINTF_PARAMS(2, 3);
//
// For va_list based printf style functions, specfy 0 as the second parameter.
// Example:
//   void foobarv(int a, const char *fmt, va_list ap) PRINTF_PARAMS(2, 0);
//
// Note that 'this' is counted as a method argument. For non-static methods,
// add 1 to the indices.
#if defined(__GNUC__)
  #define PRINTF_PARAMS(...) __attribute__ ((format (printf, __VA_ARGS__)))
  #define SCANF_PARAMS(...) __attribute__ ((format (scanf, __VA_ARGS__)))
#else
  #define PRINTF_PARAMS(...)
  #define SCANF_PARAMS(...)
#endif

// DLL import / export
#if defined(__GNUC__)
	#define DLL_EXPORT __attribute__ ((visibility("default")))
	#define DLL_IMPORT __attribute__ ((visibility("default")))
#else
	#define DLL_EXPORT __declspec(dllexport)
	#define DLL_IMPORT __declspec(dllimport)
#endif

//////////////////////////////////////////////////////////////////////////
// Define BIT macro for use in enums and bit masks.
#define BIT(x) (1<<(x))
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Globally Used Defines.
//////////////////////////////////////////////////////////////////////////
// Platform: WIN23,WIN64,LINUX32,LINUX64,_XBOX
//////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
#define ILINE __forceinline
#else
#define ILINE inline
#endif

#ifdef _MSC_VER
#define DEPRICATED __declspec(deprecated)
#else
#define DEPRICATED
#endif

//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////
typedef signed char         int8;
typedef signed short        int16;
typedef signed int          int32;
typedef int64_t             int64;
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef uint64_t            uint64;
typedef float               f32;
typedef double              f64;
typedef double              real;  //biggest float-type on this machine
typedef unsigned long       DWORD;

#ifdef _MSC_VER

#ifdef WIN64
typedef __int64 INT_PTR, *PINT_PTR;
typedef unsigned __int64 UINT_PTR, *PUINT_PTR;

typedef __int64 LONG_PTR, *PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
#else
typedef __w64 int INT_PTR, *PINT_PTR;
typedef __w64 unsigned int UINT_PTR, *PUINT_PTR;

typedef __w64 long LONG_PTR, *PLONG_PTR;
typedef __w64 unsigned long ULONG_PTR, *PULONG_PTR;
#endif

#else

typedef intptr_t INT_PTR, *PINT_PTR;
typedef uintptr_t UINT_PTR, *PUINT_PTR;

typedef intptr_t LONG_PTR, *PLONG_PTR;
typedef uintptr_t ULONG_PTR, *PULONG_PTR;

#endif

typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if(p) { delete (p); (p)=NULL; } }
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete [] (p); (p)=NULL; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#include <stdio.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <algorithm>

#define CrySwprintf _snwprintf

#include "CryString.h"
typedef CryStringT<char> string;
typedef CryStringT<wchar_t> wstring;

// Include MultiThreading support.
#include "MultiThread.h"

// Include Big/Small endianess conversion.
#include "Endian.h"

// Include array.
#include "CryArray.h"

// Fixed-Sized (stack based string)
// put after the platform wrappers because of missing _wcsicmp/_wcsnicmp functions
#include "CryFixedString.h"

// need this in a common header file and any other file would be too misleading
enum ETriState
{
	eTS_false,
	eTS_true,
	eTS_maybe
};

// needed by both IRenderer.h and I3DEngine.h
enum eSplashType
{
	EST_Water,
};
