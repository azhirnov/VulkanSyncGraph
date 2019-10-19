// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Math/Math.h"

namespace VSA
{

	//
	// Array View
	//

	template <typename T>
	struct ArrayView
	{
	// types
	public:
		using value_type		= T;
		using iterator			= T *;
		using const_iterator	= T const *;


	// variables
	private:
		union {
			T const *	_array;
			T const		(*_dbgView)[400];		// debug viewer, don't use this field!
		};
		size_t		_count	= 0;


	// methods
	public:
		ArrayView () : _array{null} {}
		
		ArrayView (T const* ptr, size_t count) : _array{ptr}, _count{count}
		{
			ASSERT( (_count == 0) or (_array != null) ); 
		}

		ArrayView (std::initializer_list<T> list) : _array{list.begin()}, _count{list.size()} {}

		template <typename AllocT>
		ArrayView (const std::vector<T,AllocT> &vec) : _array{vec.data()}, _count{vec.size()}
		{
			ASSERT( (_count == 0) or (_array != null) ); 
		}

		template <size_t S>
		ArrayView (const StaticArray<T,S> &arr) : _array{arr.data()}, _count{arr.size()} {}

		template <size_t S>
		ArrayView (const T (&arr)[S]) : _array{arr}, _count{S} {}

		ND_ explicit operator Array<T> ()			const	{ return Array<T>{ begin(), end() }; }

		ND_ size_t			size ()					const	{ return _count; }
		ND_ bool			empty ()				const	{ return _count == 0; }
		ND_ T const *		data ()					const	{ return _array; }

		ND_ T const &		operator [] (size_t i)	const	{ ASSERT( i < _count );  return _array[i]; }

		ND_ const_iterator	begin ()				const	{ return _array; }
		ND_ const_iterator	end ()					const	{ return _array + _count; }

		ND_ T const&		front ()				const	{ ASSERT( _count > 0 );  return _array[0]; }
		ND_ T const&		back ()					const	{ ASSERT( _count > 0 );  return _array[_count-1]; }


		ND_ bool  operator == (ArrayView<T> rhs) const
		{
			if ( size() != rhs.size() )
				return false;

			for (size_t i = 0; i < size(); ++i) {
				if ( not (_array[i] == rhs[i]) )
					return false;
			}
			return true;
		}

		ND_ bool  operator >  (ArrayView<T> rhs) const
		{
			if ( size() != rhs.size() )
				return size() > rhs.size();

			for (size_t i = 0; i < size(); ++i)
			{
				if ( _array[i] != rhs[i] )
					return _array[i] > rhs[i];
			}
			return true;
		}
		
		ND_ bool  operator != (ArrayView<T> rhs) const	{ return not (*this == rhs); }
		ND_ bool  operator <  (ArrayView<T> rhs) const	{ return (rhs > *this); }
		ND_ bool  operator >= (ArrayView<T> rhs) const	{ return not (*this < rhs); }
		ND_ bool  operator <= (ArrayView<T> rhs) const	{ return not (*this > rhs); }


		ND_ ArrayView<T> section (size_t first, size_t count) const
		{
			return first < size() ?
					ArrayView<T>{ data() + first, Min(size() - first, count) } :
					ArrayView<T>{};
		}
	};


}	// VSA


namespace std
{
	template <typename T>
	struct hash< VSA::ArrayView<T> >
	{
		ND_ size_t  operator () (const VSA::ArrayView<T> &value) const
		{
			if constexpr ( VSA_FAST_HASH and VSA::IsPOD<T> )
			{
				return size_t(VSA::HashOf( value.data(), value.size() * sizeof(T) ));
			}
			else
			{
				VSA::HashVal	result = VSA::HashOf( value.size() );

				for (auto& item : value) {
					result << VSA::HashOf( item );
				}
				return size_t(result);
			}
		}
	};


	template <typename T>
	struct hash< vector<T> >
	{
		ND_ size_t  operator () (const vector<T> &value) const
		{
			return size_t(VSA::HashOf( VSA::ArrayView<T>{ value } ));
		}
	};


	template <typename T, size_t S>
	struct hash< array<T,S> >
	{
		ND_ size_t  operator () (const array<T,S> &value) const
		{
			return size_t(VSA::HashOf( VSA::ArrayView<T>{ value } ));
		}
	};

}	// std
