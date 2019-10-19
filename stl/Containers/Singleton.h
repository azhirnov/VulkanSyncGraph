// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Memory/MemUtils.h"

namespace VSA
{

/*
=================================================
	Singleton
=================================================
*/
	template <typename T>
	ND_ inline static T*  Singleton ()
	{
		static T inst;
		return AddressOf( inst );
	}

}	// VSA
