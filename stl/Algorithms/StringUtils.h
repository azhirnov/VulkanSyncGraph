// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Math/Math.h"
#include "stl/Math/Bytes.h"
#include "stl/Math/Color.h"
#include "stl/Algorithms/EnumUtils.h"
#include "stl/Algorithms/ArrayUtils.h"
#include "stl/Memory/MemUtils.h"
#include <chrono>
#include <sstream>

namespace VSA
{
	using namespace std::string_literals;

/*
=================================================
	operator << (String, String)
	operator << (String, StringView)
	operator << (String, CStyleString)
	operator << (String, char)
=================================================
*/
	template <typename T>
	forceinline BasicString<T>&&  operator << (BasicString<T> &&lhs, const BasicString<T> &rhs)
	{
		return std::move( std::move(lhs).append( rhs ));
	}

	template <typename T>
	forceinline BasicString<T>&  operator << (BasicString<T> &lhs, const std::basic_string<T> &rhs)
	{
		return lhs.append( rhs );
	}

	template <typename T>
	forceinline BasicString<T>&&  operator << (BasicString<T> &&lhs, const BasicStringView<T> &rhs)
	{
		return std::move( std::move(lhs).append( BasicString<T>{rhs} ));
	}

	template <typename T>
	forceinline BasicString<T>&  operator << (BasicString<T> &lhs, const BasicStringView<T> &rhs)
	{
		return lhs.append( BasicString<T>{rhs} );
	}

	template <typename T>
	forceinline BasicString<T>&&  operator << (BasicString<T> &&lhs, T const * const rhs)
	{
		return std::move( std::move(lhs).append( rhs ));
	}

	template <typename T>
	forceinline BasicString<T>&  operator << (BasicString<T> &lhs, T const * const rhs)
	{
		return lhs.append( rhs );
	}

	template <typename T>
	forceinline BasicString<T>&&  operator << (BasicString<T> &&lhs, const T rhs)
	{
		return std::move( std::move(lhs) += rhs );
	}

	template <typename T>
	forceinline BasicString<T>&  operator << (BasicString<T> &lhs, const T rhs)
	{
		return (lhs += rhs);
	}
//-----------------------------------------------------------------------------



/*
=================================================
	IsUpperCase
=================================================
*/
	ND_ forceinline const char  IsUpperCase (const char c)
	{
		return (c >= 'A' and c <= 'Z');
	}

/*
=================================================
	IsLowerCase
=================================================
*/
	ND_ forceinline const char  IsLowerCase (const char c)
	{
		return (c >= 'a' and c <= 'z');
	}

/*
=================================================
	ToLowerCase
=================================================
*/
	ND_ forceinline const char  ToLowerCase (const char c)
	{
		return IsUpperCase( c ) ? (c - 'A' + 'a') : c;
	}

/*
=================================================
	ToUpperCase
=================================================
*/
	ND_ forceinline const char  ToUpperCase (const char c)
	{
		return IsLowerCase( c ) ? (c - 'a' + 'A') : c;
	}

/*
=================================================
	HasSubString
----
	returns 'true' if 'str' has substring 'substr',
	comparison is case sansitive.
=================================================
*/
	ND_ inline bool  HasSubString (StringView str, StringView substr)
	{
		return (str.find( substr ) != StringView::npos);
	}

/*
=================================================
	HasSubStringIC
----
	returns 'true' if 'str' has substring 'substr',
	comparison is case insansitive.
=================================================
*/
	ND_ inline bool  HasSubStringIC (StringView str, StringView substr)
	{
		if ( str.empty() or substr.empty() )
			return false;

		for (size_t i = 0, j = 0; i < str.length(); ++i)
		{
			while ( i+j < str.length() and j < substr.length() and
					ToLowerCase( substr[j] ) == ToLowerCase( str[i+j] ) )
			{
				++j;
				if ( j >= substr.length() )
					return true;
			}
			j = 0;
		}
		return false;
	}
	
/*
=================================================
	StartsWith
----
	returns 'true' if 'str' starts with substring 'substr',
	comparison is case sansitive.
=================================================
*/
	ND_ inline bool  StartsWith (StringView str, StringView substr)
	{
		if ( str.length() < substr.length() )
			return false;

		for (size_t i = 0; i < substr.length(); ++i)
		{
			if ( str[i] != substr[i] )
				return false;
		}
		return true;
	}
	
/*
=================================================
	StartsWithIC
----
	returns 'true' if 'str' starts with substring 'substr',
	comparison is case insansitive.
=================================================
*/
	ND_ inline bool  StartsWithIC (StringView str, StringView substr)
	{
		if ( str.length() < substr.length() )
			return false;

		for (size_t i = 0; i < substr.length(); ++i)
		{
			if ( ToLowerCase(str[i]) != ToLowerCase(substr[i]) )
				return false;
		}
		return true;
	}
	
/*
=================================================
	EndsWith
----
	returns 'true' if 'str' ends with substring 'substr',
	comparison is case sansitive.
=================================================
*/
	ND_ inline bool  EndsWith (StringView str, StringView substr)
	{
		if ( str.length() < substr.length() )
			return false;

		for (size_t i = 1; i <= substr.length(); ++i)
		{
			if ( str[str.length() - i] != substr[substr.length() - i] )
				return false;
		}
		return true;
	}
	
/*
=================================================
	EndsWithIC
----
	returns 'true' if 'str' ends with substring 'substr',
	comparison is case insansitive.
=================================================
*/
	ND_ inline bool  EndsWithIC (StringView str, StringView substr)
	{
		if ( str.length() < substr.length() )
			return false;

		for (size_t i = 1; i <= substr.length(); ++i)
		{
			if ( ToLowerCase(str[str.length() - i]) != ToLowerCase(substr[substr.length() - i]) )
				return false;
		}
		return true;
	}
	
/*
=================================================
	FindAndReplace
=================================================
*/
	inline uint  FindAndReplace (INOUT String& str, StringView oldStr, StringView newStr)
	{
		String::size_type	pos		= 0;
		uint				count	= 0;

		while ( (pos = StringView{str}.find( oldStr, pos )) != StringView::npos )
		{
			str.replace( pos, oldStr.length(), newStr.data() );
			pos += newStr.length();
			++count;
		}
		return count;
	}
//-----------------------------------------------------------------------------



/*
=================================================
	ToString
=================================================
*/
	template <typename T>
	ND_ forceinline EnableIf<not IsEnum<T>, String>  ToString (const T &value)
	{
		return std::to_string( value );
	}

	template <typename E>
	ND_ forceinline EnableIf<IsEnum<E>, String>  ToString (const E &value)
	{
		using T = Conditional< (sizeof(E) > sizeof(uint32_t)), uint32_t, uint64_t >;

		return std::to_string( T(value) );
	}

	ND_ forceinline String  ToString (const bool &value)
	{
		return value ? "true" : "false";
	}
	
/*
=================================================
	ToString
=================================================
*/
	template <int Radix, typename T>
	ND_ forceinline EnableIf< IsEnum<T> or IsInteger<T>, String>  ToString (const T &value)
	{
		if constexpr ( Radix == 16 )
		{
			std::stringstream	str;
			str << std::hex << BitCast<NearUInt<T>>( value );
			return str.str();
		}else{
			//STATIC_ASSERT( false, "not supported, yet" );
		}
	}

/*
=================================================
	ToString (double)
=================================================
*/
	ND_ inline String  ToString (const double &value, uint fractParts)
	{
		ASSERT( fractParts > 0 and fractParts < 100 );
		fractParts = Clamp( fractParts, 1u, 99u );

		const char	fmt[8]  = {'%', '0', '.', char('0' + fractParts/10), char('0' + fractParts%10), 'f', '\0' };
		char		buf[32] = {};

		const int	len = std::snprintf( buf, CountOf(buf), fmt, value );
		ASSERT( len > 0 );
		return buf;
	}
	
/*
=================================================
	ToString (RGBAColor)
=================================================
*/
	template <typename T>
	ND_ inline String  ToString (const RGBAColor<T> &value)
	{
		String	str = "( "s;

		for (uint i = 0; i < 4; ++i)
		{
			if ( i > 0 )	str << ", ";
			str << ToString( value[i] );
		}
		str << " )";
		return str;
	}

/*
=================================================
	ToString (Bytes)
=================================================
*/
	template <typename T>
	ND_ inline String  ToString (const Bytes<T> &value)
	{
		const T	kb	= T(1) << 12;
		const T mb	= T(1) << 22;
		const T	gb	= T(1) << Min( T(32), T(sizeof(T)*8)-1 );
		const T	val	= T(value);

		String	str;

		if ( val < kb )	str << ToString( val ) << " b";			else
		if ( val < mb )	str << ToString( val >> 10 ) << " Kb";	else
		if ( val < gb )	str << ToString( val >> 20 ) << " Mb";	else
						str << ToString( val >> 30 ) << " Gb";
		return str;
	}

/*
=================================================
	ToString (chrono::duration)
=================================================
*/
	template <typename T, typename Duration>
	ND_ inline String  ToString (const std::chrono::duration<T,Duration> &value, uint precission = 2)
	{
		using SecondsD_t  = std::chrono::duration<double>;
		using MicroSecD_t = std::chrono::duration<double, std::micro>;
		using NanoSecD_t  = std::chrono::duration<double, std::nano>;

		const auto	time = std::chrono::duration_cast<SecondsD_t>( value ).count();
		String		str;

		if ( time > 59.0 * 60.0 )
			str << ToString( time * (1.0/3600.0), precission ) << " h";
		else
		if ( time > 59.0 )
			str << ToString( time * (1.0/60.0), precission ) << " m";
		else
		if ( time > 1.0e-1 )
			str << ToString( time, precission ) << " s";
		else
		if ( time > 1.0e-4 )
			str << ToString( time * 1.0e+3, precission ) << " ms";
		else
		if ( time > 1.0e-7 )
			str << ToString( std::chrono::duration_cast<MicroSecD_t>( value ).count(), precission ) << " us";
		else
			str << ToString( std::chrono::duration_cast<NanoSecD_t>( value ).count(), precission ) << " ns";

		return str;
	}


}	// VSA
