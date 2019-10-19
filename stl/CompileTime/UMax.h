// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'
/*
	UMax constant is maximum value of unsigned integer type.
*/

#pragma once

namespace VSA
{
namespace _vsa_hidden_
{
	struct _UMax
	{
		template <typename T>
		ND_ constexpr operator const T () const
		{
			STATIC_ASSERT( T(~T(0)) > T(0) );
			return T(~T(0));
		}
			
		template <typename T>
		ND_ friend constexpr bool operator == (const T& left, const _UMax &right)
		{
			return T(right) == left;
		}
			
		template <typename T>
		ND_ friend constexpr bool operator != (const T& left, const _UMax &right)
		{
			return T(right) != left;
		}
	};

}	// _vsa_hidden_


	static constexpr _vsa_hidden_::_UMax		UMax {};

}	// VSA
