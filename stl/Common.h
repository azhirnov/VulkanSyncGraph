// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'
/*
	Frame Graph Core library
*/

#pragma once

#include "stl/Defines.h"

#include <vector>
#include <string>
#include <array>
#include <memory>		// shared_ptr, weak_ptr, unique_ptr
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <bitset>
#include <cstring>
#include <cmath>
#include <malloc.h>
#include <atomic>
#include <optional>
#include <string_view>

#include "stl/Log/Log.h"
#include "stl/Algorithms/Hash.h"
#include "stl/CompileTime/TypeTraits.h"
#include "stl/CompileTime/UMax.h"
#include "stl/CompileTime/DefaultType.h"


namespace VSA
{
	using uint = uint32_t;

							using String		= std::string;
	template <typename T>	using BasicString	= std::basic_string< T >;

	template <typename T>	using Array			= std::vector< T >;

	template <typename T>	using UniquePtr		= std::unique_ptr< T >;

	template <typename T>	using SharedPtr		= std::shared_ptr< T >;
	template <typename T>	using WeakPtr		= std::weak_ptr< T >;

	template <typename T>	using Deque			= std::deque< T >;

	template <size_t N>		using BitSet		= std::bitset< N >;
	
	template <typename T>	using Optional		= std::optional< T >;

	template <typename...T>	using Tuple			= std::tuple< T... >;
	
							using StringView		= std::string_view;
	template <typename T>	using BasicStringView	= std::basic_string_view<T>;


	template <typename T,
			  size_t ArraySize>
	using StaticArray	= std::array< T, ArraySize >;


	template <typename FirstT,
			  typename SecondT>
	using Pair = std::pair< FirstT, SecondT >;
	

	template <typename T,
			  typename Hasher = std::hash<T>>
	using HashSet = std::unordered_set< T, Hasher >;


	template <typename Key,
			  typename Value,
			  typename Hasher = std::hash<Key>>
	using HashMap = std::unordered_map< Key, Value, Hasher >;
	
	template <typename Key,
			  typename Value,
			  typename Hasher = std::hash<Key>>
	using HashMultiMap = std::unordered_multimap< Key, Value, Hasher >;

	template <typename T>
	using Function = std::function< T >;


	template <typename ...Args>
	ND_ inline auto  MakeTuple (Args&& ...args)
	{
		return std::make_tuple( std::forward<Args>(args)... );
	}

}	// VSA
