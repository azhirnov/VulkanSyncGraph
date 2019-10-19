// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/CompileTime/TypeList.h"

namespace VSA
{
	namespace _vsa_hidden_
	{

		template <typename T>
		struct _FuncInfo {};
		
		template <typename T>
		struct _FuncInfo< T * > {};
		
		template <typename T, typename Class>
		struct _FuncInfo< T (Class::*) >	{};
		
		
		template <typename Result, typename ...Args>
		struct _FuncInfo< Result (Args...) >
		{
			using args		= VSA::TypeList< Args... >;
			using result	= Result;
			using type		= Result (*) (Args...);
			using clazz		= void;

			static constexpr bool	is_const	= false;
			static constexpr bool	is_volatile	= false;
		};
		
		template <typename Result, typename ...Args>
		struct _FuncInfo< Result (*) (Args...) >
		{
			using args		= VSA::TypeList< Args... >;
			using result	= Result;
			using type		= Result (*) (Args...);
			using clazz		= void;
		
			static constexpr bool	is_const	= false;
			static constexpr bool	is_volatile	= false;
		};
		
		template <typename Class, typename Result, typename ...Args>
		struct _FuncInfo< Result (Class::*) (Args...) >
		{
			using args		= VSA::TypeList< Args... >;
			using result	= Result;
			using type		= Result (Class::*) (Args...);
			using clazz		= Class;
		
			static constexpr bool	is_const	= false;
			static constexpr bool	is_volatile	= false;
		};
		
		template <typename Result, typename ...Args>
		struct _FuncInfo< Function< Result (Args...) > >
		{
			using args		= VSA::TypeList< Args... >;
			using result	= Result;
			using type		= Result (*) (Args...);
			using clazz		= void;

			static constexpr bool	is_const	= false;
			static constexpr bool	is_volatile	= false;
		};

		#define _DECL_FUNC_INFO( _cv_qual_ ) \
			template <typename Class, typename Result, typename ...Args> \
			struct _FuncInfo< Result (Class::*) (Args...) _cv_qual_ > \
			{ \
				using args		= VSA::TypeList< Args... >; \
				using result	= Result; \
				using type		= Result (Class::*) (Args...) _cv_qual_; \
				using clazz		= Class; \
				\
				static constexpr bool	is_const	= std::is_const_v< _cv_qual_ int >; \
				static constexpr bool	is_volatile	= std::is_volatile_v< _cv_qual_ int >; \
			};
		_DECL_FUNC_INFO( const );
		_DECL_FUNC_INFO( volatile );
		_DECL_FUNC_INFO( const volatile );
		_DECL_FUNC_INFO( noexcept );
		_DECL_FUNC_INFO( const noexcept );
		_DECL_FUNC_INFO( volatile noexcept );
		_DECL_FUNC_INFO( const volatile noexcept );
		_DECL_FUNC_INFO( & );
		_DECL_FUNC_INFO( const & );
		_DECL_FUNC_INFO( volatile & );
		_DECL_FUNC_INFO( const volatile & );
		_DECL_FUNC_INFO( & noexcept );
		_DECL_FUNC_INFO( const & noexcept );
		_DECL_FUNC_INFO( volatile & noexcept );
		_DECL_FUNC_INFO( const volatile & noexcept );
		_DECL_FUNC_INFO( && );
		_DECL_FUNC_INFO( const && );
		_DECL_FUNC_INFO( volatile && );
		_DECL_FUNC_INFO( const volatile && );
		_DECL_FUNC_INFO( && noexcept );
		_DECL_FUNC_INFO( const && noexcept );
		_DECL_FUNC_INFO( volatile && noexcept );
		_DECL_FUNC_INFO( const volatile && noexcept );
		#undef _DECL_FUNC_INFO

		
		template < typename T, bool L >
		struct _FuncInfo2 {
			using type = _FuncInfo<T>;
		};
    
		template < typename T >
		struct _FuncInfo2<T, true> {
			using type = _FuncInfo< decltype(&T::operator()) >;
		};

		template < typename T >
		struct _FuncInfo3 {
			using type = typename _FuncInfo2< T, std::is_class_v<T> >::type;
		};

	}	// _vsa_hidden_


	template <typename T>
	using FunctionInfo = typename _vsa_hidden_::_FuncInfo3<T>::type;

}	// VSA
