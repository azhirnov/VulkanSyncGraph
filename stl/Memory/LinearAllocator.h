// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Math/Math.h"
#include "stl/Math/Bytes.h"
#include "stl/Memory/UntypedAllocator.h"

namespace VSA
{
	template <typename AllocatorType>				struct LinearAllocator;
	template <typename T, typename AllocatorType>	struct StdLinearAllocator;
	template <typename AllocatorType>				struct UntypedLinearAllocator;



	//
	// Linear Pool Allocator
	//

	template <typename AllocatorType = UntypedAlignedAllocator>
	struct LinearAllocator final
	{
	// types
	private:
		struct Block
		{
			void *		ptr			= null;
			BytesU		size;		// used memory size
			BytesU		capacity;	// size of block
		};

		using Allocator_t	= AllocatorType;
		using Blocks_t		= std::vector< Block >;


	// variables
	private:
		Blocks_t					_blocks;
		BytesU						_blockSize	= 1024_b;
		Allocator_t					_alloc;
		static constexpr BytesU		_ptrAlign	= SizeOf<void *>;


	// methods
	public:
		LinearAllocator () {}
		
		explicit LinearAllocator (const Allocator_t &alloc) : _alloc{alloc}
		{
			_blocks.reserve( 16 );
		}
		
		LinearAllocator (LinearAllocator &&other) :
			_blocks{ std::move(other._blocks) },
			_blockSize{ other._blockSize },
			_alloc{ std::move(other._alloc) }
		{}

		LinearAllocator (const LinearAllocator &) = delete;

		LinearAllocator& operator = (const LinearAllocator &) = delete;


		LinearAllocator& operator = (LinearAllocator &&rhs)
		{
			Release();
			_blocks		= std::move(rhs._blocks);
			_blockSize	= rhs._blockSize;
			_alloc		= std::move(rhs._alloc);
			return *this;
		}


		~LinearAllocator ()
		{
			Release();
		}


		void SetBlockSize (BytesU size)
		{
			_blockSize = size;
		}


		ND_ VSA_ALLOCATOR void*  Alloc (const BytesU size, const BytesU align)
		{
			for (auto& block : _blocks)
			{
				BytesU	offset	= AlignToLarger( size_t(block.ptr) + block.size, align ) - size_t(block.ptr);

				if ( size <= (block.capacity - offset) )
				{
					block.size = offset + size;
					return block.ptr + offset;
				}
			}

			BytesU	block_size	= _blockSize * (1 + _blocks.size()/2);
					block_size	= size*2 < block_size ? block_size : size*2;
			auto&	block		= _blocks.emplace_back(Block{ _alloc.Allocate( block_size, _ptrAlign ), 0_b, block_size });	// TODO: check for null
			BytesU	offset		= AlignToLarger( size_t(block.ptr) + block.size, align ) - size_t(block.ptr);

			block.size = offset + size;
			return block.ptr + offset;
		}


		template <typename T>
		ND_ VSA_ALLOCATOR T*  Alloc (size_t count = 1)
		{
			return BitCast<T *>( Alloc( SizeOf<T> * count, AlignOf<T> ));
		}


		void Discard ()
		{
			for (auto& block : _blocks) {
				block.size = 0_b;
			}
		}

		void Release ()
		{
			for (auto& block : _blocks) {
				_alloc.Deallocate( block.ptr, block.capacity, _ptrAlign );
			}
			_blocks.clear();
		}
	};



	//
	// Untyped Linear Pool Allocator
	//
	
	template <typename AllocatorType = UntypedAlignedAllocator>
	struct UntypedLinearAllocator final
	{
	// types
	public:
		using LinearAllocator_t	= LinearAllocator< AllocatorType >;
		using Allocator_t		= AllocatorType;
		using Self				= UntypedLinearAllocator< AllocatorType >;

		template <typename T>
		using StdAllocator_t	= StdLinearAllocator< T, AllocatorType >;


	// variables
	private:
		LinearAllocator_t&	_alloc;
		

	// methods
	public:
		UntypedLinearAllocator (Self &&other) : _alloc{other._alloc} {}
		UntypedLinearAllocator (const Self &other) : _alloc{other._alloc} {}
		UntypedLinearAllocator (LinearAllocator_t &alloc) : _alloc{alloc} {}


		ND_ VSA_ALLOCATOR void*  Allocate (BytesU size, BytesU align)
		{
			return _alloc.Alloc( size, align );
		}

		void  Deallocate (void *, BytesU)
		{}

		void  Deallocate (void *, BytesU, BytesU)
		{}

		ND_ LinearAllocator_t&  GetAllocatorRef () const
		{
			return _alloc;
		}

		ND_ bool operator == (const Self &rhs) const
		{
			return &_alloc == &rhs._alloc;
		}
	};
	


	//
	// Std Linear Pool Allocator
	//

	template <typename T,
			  typename AllocatorType = UntypedAlignedAllocator>
	struct StdLinearAllocator
	{
	// types
	public:
		using value_type			= T;
		using LinearAllocator_t		= LinearAllocator< AllocatorType >;
		using Allocator_t			= AllocatorType;
		using Self					= StdLinearAllocator< T, AllocatorType >;
		using UntypedAllocator_t	= UntypedLinearAllocator< AllocatorType >;


	// variables
	private:
		LinearAllocator_t&	_alloc;


	// methods
	public:
		StdLinearAllocator (LinearAllocator_t &alloc) : _alloc{alloc} {}
		StdLinearAllocator (const UntypedAllocator_t &alloc) : _alloc{alloc.GetAllocatorRef()} {}

		StdLinearAllocator (Self &&other) : _alloc{other._alloc} {}
		StdLinearAllocator (const Self &other) : _alloc{other._alloc} {}
		
		template <typename B>
		StdLinearAllocator (const StdLinearAllocator<B,Allocator_t>& other) : _alloc{other.GetAllocatorRef()} {}

		Self& operator = (const Self &) = delete;

		
		ND_ VSA_ALLOCATOR T*  allocate (const size_t count)
		{
			return _alloc.template Alloc<T>( count );
		}

		void deallocate (T * const, const size_t)
		{
		}

		ND_ Self  select_on_container_copy_construction() const
		{
			return Self{ _alloc };
		}

		ND_ LinearAllocator_t&  GetAllocatorRef () const
		{
			return _alloc;
		}

		ND_ bool operator == (const Self &rhs) const
		{
			return &_alloc == &rhs._alloc;
		}
	};


}	// VSA
