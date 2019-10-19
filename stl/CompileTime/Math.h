// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Common.h"
#include "stl/CompileTime/TypeTraits.h"

namespace VSA
{

/*
=================================================
	CT_IntLog2
=================================================
*/
namespace _vsa_hidden_
{
	template <typename T, T X, uint Bit>
	struct _IntLog2 {
		static const int	value = int((X >> Bit) != 0) + _IntLog2<T, X, Bit-1 >::value;
	};

	template <typename T, T X>
	struct _IntLog2< T, X, 0 > {
		static const int	value = 0;
	};

}	// _vsa_hidden_

	template <auto X>
	static constexpr int	CT_IntLog2 = (X ? _vsa_hidden_::_IntLog2< decltype(X), X, sizeof(X)*8-1 >::value : -1);

	
/*
=================================================
	CT_Pow
=================================================
*/
	template <auto Power, typename T>
	inline constexpr T  CT_Pow (const T &base)
	{
		STATIC_ASSERT( IsInteger<T> and IsInteger<decltype(Power)> and Power >= 0 );

		if constexpr( Power == 0 )
		{
			VSA_UNUSED( base );
			return 1;
		}
		else
			return CT_Pow<Power-1>( base ) * base;
	}


}	// VSA
