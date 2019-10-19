// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once


#if defined(DEBUG) || defined(_DEBUG)
#	define VSA_DEBUG
#else
#	define VSA_RELEASE
#endif

#include "stl/Config.h"


#ifdef COMPILER_MSVC
#	define and		&&
#	define or		||
#	define not		!
#endif


// mark output and input-output function arguments
#ifndef OUT
#	define OUT
#endif

#ifndef INOUT
#	define INOUT
#endif


// no discard
#ifndef ND_
# ifdef COMPILER_MSVC
#  if _MSC_VER >= 1917
#	define ND_				[[nodiscard]]
#  else
#	define ND_
#  endif
# endif	// COMPILER_MSVC

# ifdef COMPILER_CLANG
#  if __has_feature( cxx_attributes )
#	define ND_				[[nodiscard]]
#  else
#	define ND_
#  endif
# endif	// COMPILER_CLANG

# ifdef COMPILER_GCC
#  if __has_cpp_attribute( nodiscard )
#	define ND_				[[nodiscard]]
#  else
#	define ND_
#  endif
# endif	// COMPILER_GCC

#endif	// ND_


// null pointer
#ifndef null
#	define null				nullptr
#endif


// force inline
#ifndef forceinline
# ifdef VSA_DEBUG
#	define forceinline		inline

# elif defined(COMPILER_MSVC)
#	define forceinline		__forceinline

# elif defined(COMPILER_CLANG) or defined(COMPILER_GCC)
#	define forceinline		__inline__ __attribute__((always_inline))

# else
#	pragma warning ("forceinline is not supported")
#	define forceinline		inline
# endif
#endif


// macro for unused variables
#ifndef VSA_UNUSED
# if 0 // TODO: C++17
#	define VSA_UNUSED( ... )		[[maybe_unused]]( __VA_ARGS__ )
# else
#	define VSA_UNUSED( ... )		(void)( __VA_ARGS__ )
# endif
#endif


// debug break
#ifndef VSA_PRIVATE_BREAK_POINT
# if defined(COMPILER_MSVC) and defined(VSA_DEBUG)
#	define VSA_PRIVATE_BREAK_POINT()		__debugbreak()

# elif defined(PLATFORM_ANDROID) and defined(VSA_DEBUG)
#	include <csignal>
#	define VSA_PRIVATE_BREAK_POINT()		std::raise( SIGINT )

# elif (defined(COMPILER_CLANG) or defined(COMPILER_GCC)) and defined(VSA_DEBUG)
#  if 1
#	include <exception>
#	define VSA_PRIVATE_BREAK_POINT() 		throw std::runtime_error("breakpoint")
#  else
#	include <csignal>
#	define VSA_PRIVATE_BREAK_POINT()		std::raise(SIGINT)
#  endif

# else
#	define VSA_PRIVATE_BREAK_POINT()		{}
# endif
#endif


// exit
#ifndef VSA_PRIVATE_EXIT
# if defined(PLATFORM_ANDROID)
#	define VSA_PRIVATE_EXIT()	std::terminate()
# else
#	define VSA_PRIVATE_EXIT()	::exit( EXIT_FAILURE )
# endif
#endif


// helper macro
#define VSA_PRIVATE_GETARG_0( _0_, ... )			_0_
#define VSA_PRIVATE_GETARG_1( _0_, _1_, ... )		_1_
#define VSA_PRIVATE_GETARG_2( _0_, _1_, _2_, ... )	_2_
#define VSA_PRIVATE_GETRAW( _value_ )				_value_
#define VSA_PRIVATE_TOSTRING( ... )					#__VA_ARGS__
#define VSA_PRIVATE_UNITE_RAW( _arg0_, _arg1_ )		VSA_PRIVATE_UNITE( _arg0_, _arg1_ )
#define VSA_PRIVATE_UNITE( _arg0_, _arg1_ )			_arg0_ ## _arg1_


// debug only check
#ifndef ASSERT
# ifdef VSA_DEBUG
#	define ASSERT				CHECK
# else
#	define ASSERT( ... )		{}
# endif
#endif


// function name
#ifdef COMPILER_MSVC
#	define VSA_FUNCTION_NAME			__FUNCTION__

#elif defined(COMPILER_CLANG) or defined(COMPILER_GCC)
#	define VSA_FUNCTION_NAME			__func__

#else
#	define VSA_FUNCTION_NAME			"unknown function"
#endif


// branch prediction optimization
#if 0 // TODO: C++20
#	define if_likely( ... )		[[likely]] if ( __VA_ARGS__ )
#	define if_unlikely( ... )	[[unlikely]] if ( __VA_ARGS__ )

#elif defined(COMPILER_CLANG) or defined(COMPILER_GCC)
#	define if_likely( ... )		if ( __builtin_expect( !!(__VA_ARGS__), 1 ))
#	define if_unlikely( ... )	if ( __builtin_expect( !!(__VA_ARGS__), 0 ))
#else
	// not supported
#	define if_likely( ... )		if ( __VA_ARGS__ )
#	define if_unlikely( ... )	if ( __VA_ARGS__ )
#endif


// debug only scope
#ifndef DEBUG_ONLY
# ifdef VSA_DEBUG
#	define DEBUG_ONLY( ... )		__VA_ARGS__
# else
#	define DEBUG_ONLY( ... )
# endif
#endif


// log
// (text, file, line)
#ifndef VSA_LOGD
# ifdef VSA_DEBUG
#	define VSA_LOGD	VSA_LOGI
# else
#	define VSA_LOGD( ... )
# endif
#endif

#ifndef VSA_LOGI
#	define VSA_LOGI( ... ) \
			VSA_PRIVATE_LOGI(VSA_PRIVATE_GETARG_0( __VA_ARGS__, "" ), \
							 VSA_PRIVATE_GETARG_1( __VA_ARGS__, __FILE__ ), \
							 VSA_PRIVATE_GETARG_2( __VA_ARGS__, __FILE__, __LINE__ ))
#endif

#ifndef VSA_LOGE
#	define VSA_LOGE( ... ) \
			VSA_PRIVATE_LOGE(VSA_PRIVATE_GETARG_0( __VA_ARGS__, "" ), \
							 VSA_PRIVATE_GETARG_1( __VA_ARGS__, __FILE__ ), \
							 VSA_PRIVATE_GETARG_2( __VA_ARGS__, __FILE__, __LINE__ ))
#endif


// check function return value
#ifndef CHECK
#	define VSA_PRIVATE_CHECK( _expr_, _text_ ) \
		{if_likely (( _expr_ )) {} \
		 else { \
			VSA_LOGE( _text_ ); \
		}}

#   define CHECK( _func_ ) \
		VSA_PRIVATE_CHECK( (_func_), VSA_PRIVATE_TOSTRING( _func_ ) )
#endif


// check function return value and return error code
#ifndef CHECK_ERR
#	define VSA_PRIVATE_CHECK_ERR( _expr_, _ret_ ) \
		{if_likely (( _expr_ )) {}\
		  else { \
			VSA_LOGE( VSA_PRIVATE_TOSTRING( _expr_ ) ); \
			return (_ret_); \
		}}

#	define CHECK_ERR( ... ) \
		VSA_PRIVATE_CHECK_ERR( VSA_PRIVATE_GETARG_0( __VA_ARGS__ ), VSA_PRIVATE_GETARG_1( __VA_ARGS__, ::VSA::Default ) )
#endif


// check function return value and exit
#ifndef CHECK_FATAL
#	define CHECK_FATAL( _expr_ ) \
		{if_likely (( _expr_ )) {}\
		  else { \
			VSA_LOGE( VSA_PRIVATE_TOSTRING( _expr_ ) ); \
			VSA_PRIVATE_EXIT(); \
		}}
#endif


// return error code
#ifndef RETURN_ERR
#	define VSA_PRIVATE_RETURN_ERR( _text_, _ret_ ) \
		{ VSA_LOGE( _text_ );  return (_ret_); }

#	define RETURN_ERR( ... ) \
		VSA_PRIVATE_RETURN_ERR( VSA_PRIVATE_GETARG_0( __VA_ARGS__ ), VSA_PRIVATE_GETARG_1( __VA_ARGS__, ::VSA::Default ) )
#endif


// compile time assert
#ifndef STATIC_ASSERT
#	define STATIC_ASSERT( ... ) \
		static_assert(	VSA_PRIVATE_GETRAW( VSA_PRIVATE_GETARG_0( __VA_ARGS__ ) ), \
						VSA_PRIVATE_GETRAW( VSA_PRIVATE_GETARG_1( __VA_ARGS__, VSA_PRIVATE_TOSTRING(__VA_ARGS__) ) ) )
#endif


// bit operators
#define VSA_BIT_OPERATORS( _type_ ) \
	ND_ constexpr _type_  operator |  (_type_ lhs, _type_ rhs)	{ return _type_( VSA::EnumToUInt(lhs) | VSA::EnumToUInt(rhs) ); } \
	ND_ constexpr _type_  operator &  (_type_ lhs, _type_ rhs)	{ return _type_( VSA::EnumToUInt(lhs) & VSA::EnumToUInt(rhs) ); } \
	\
	constexpr _type_&  operator |= (_type_ &lhs, _type_ rhs)	{ return lhs = _type_( VSA::EnumToUInt(lhs) | VSA::EnumToUInt(rhs) ); } \
	constexpr _type_&  operator &= (_type_ &lhs, _type_ rhs)	{ return lhs = _type_( VSA::EnumToUInt(lhs) & VSA::EnumToUInt(rhs) ); } \
	\
	ND_ constexpr _type_  operator ~ (_type_ lhs)				{ return _type_(~VSA::EnumToUInt(lhs)); } \
	ND_ constexpr bool   operator ! (_type_ lhs)				{ return not VSA::EnumToUInt(lhs); } \
	

// enable/disable checks for enums
#ifdef COMPILER_MSVC
#	define BEGIN_ENUM_CHECKS() \
		__pragma (warning (push)) \
		__pragma (warning (error: 4061)) /*enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label*/ \
		__pragma (warning (error: 4062)) /*enumerator 'identifier' in switch of enum 'enumeration' is not handled*/ \
		__pragma (warning (error: 4063)) /*case 'number' is not a valid value for switch of enum 'type'*/ \

#	define END_ENUM_CHECKS() \
		__pragma (warning (pop)) \

#elif defined(COMPILER_CLANG) or defined(COMPILER_GCC)
#	define BEGIN_ENUM_CHECKS()		// TODO
#	define END_ENUM_CHECKS()	// TODO

#else
#	define BEGIN_ENUM_CHECKS()
#	define END_ENUM_CHECKS()

#endif


// allocator
#ifdef COMPILER_MSVC
#	define VSA_ALLOCATOR		__declspec( allocator )
#else
#	define VSA_ALLOCATOR
#endif


// exclusive lock
#ifndef EXLOCK
#	define EXLOCK( _syncObj_ ) \
		std::unique_lock	VSA_PRIVATE_UNITE_RAW( __scopeLock, __COUNTER__ ) { _syncObj_ }
#endif

// shared lock
#ifndef SHAREDLOCK
#	define SHAREDLOCK( _syncObj_ ) \
		std::shared_lock	VSA_PRIVATE_UNITE_RAW( __sharedLock, __COUNTER__ ) { _syncObj_ }
#endif


// thiscall, cdecl
#ifdef COMPILER_MSVC
#	define VSA_CDECL		__cdecl
#	define VSA_THISCALL		__thiscall

#elif defined(COMPILER_CLANG) or defined(COMPILER_GCC)
#	define VSA_CDECL		__attribute__((cdecl))
#	define VSA_THISCALL		__attribute__((thiscall))
#endif


// to fix compiler error C2338
#ifdef COMPILER_MSVC
#	define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif


// compile time messages
#ifndef VSA_COMPILATION_MESSAGE
#	if defined(COMPILER_CLANG)
#		define VSA_PRIVATE_MESSAGE_TOSTR(x)	#x
#		define VSA_COMPILATION_MESSAGE( _message_ )	_Pragma(VSA_PRIVATE_MESSAGE_TOSTR( GCC warning ("" _message_) ))

#	elif defined(COMPILER_MSVC)
#		define VSA_COMPILATION_MESSAGE( _message_ )	__pragma(message( _message_ ))

#	else
#		define VSA_COMPILATION_MESSAGE( _message_ )	// not supported
#	endif
#endif


// check definitions
#if defined (COMPILER_MSVC) or defined (COMPILER_CLANG)

#  ifdef VSA_DEBUG
#	pragma detect_mismatch( "VSA_DEBUG", "1" )
#  else
#	pragma detect_mismatch( "VSA_DEBUG", "0" )
#  endif

#endif	// COMPILER_MSVC or COMPILER_CLANG
