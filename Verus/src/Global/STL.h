// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	// Custom allocator for the Standard Template Library. Will try to use the allocator provided by Utils.
	template<typename T>
	class AllocatorAwareSTL
	{
		void operator=(const AllocatorAwareSTL&);

	public:
		typedef T         value_type;
		typedef size_t    size_type;
		typedef ptrdiff_t difference_type;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T& reference;
		typedef const T& const_reference;

		AllocatorAwareSTL() {}
		AllocatorAwareSTL(const AllocatorAwareSTL&) {}
		~AllocatorAwareSTL() {}

		template<typename _Other>
		AllocatorAwareSTL(const AllocatorAwareSTL<_Other>& other) {}

		template<typename U>
		struct rebind
		{
			typedef AllocatorAwareSTL<U> other;
		};

		pointer address(reference r) const
		{
			return &r;
		}

		const_pointer address(const_reference r) const
		{
			return &r;
		}

		pointer allocate(size_type n)
		{
			pointer p = static_cast<pointer>(AllocatorAware::UtilsMalloc(n * sizeof(value_type)));
			if (p)
				return p;
			else
				return static_cast<pointer>(malloc(n * sizeof(value_type)));
		}

		void deallocate(pointer p, size_type)
		{
			if (!AllocatorAware::UtilsFree(p))
				free(p);
		}

		void construct(pointer p, const value_type& val)
		{
			new(p)value_type(val);
		}

		void destroy(pointer p)
		{
			p->~value_type();
		}

		size_type max_size() const
		{
			return ULONG_MAX / sizeof(value_type);
		}
	};

	// This function will compare two allocators.
	template<typename T>
	bool operator==(const AllocatorAwareSTL<T>& l, const AllocatorAwareSTL<T>& r)
	{
		return true;
	}

	// This function will compare two allocators.
	template<typename T>
	bool operator!=(const AllocatorAwareSTL<T>& l, const AllocatorAwareSTL<T>& r)
	{
		return !(l == r);
	}
}

namespace verus
{
	template<typename T> using Vector = std::vector                     <T, AllocatorAwareSTL<T>>;
	template<typename T> using List = std::list                         <T, AllocatorAwareSTL<T>>;
	template<typename T> using Set = std::set                           <T, std::less<T>, AllocatorAwareSTL<T>>;
	template<typename T> using MultiSet = std::multiset                 <T, std::less<T>, AllocatorAwareSTL<T>>;
	template<typename T> using HashSet = std::unordered_set             <T, std::hash<T>, std::equal_to<T>, AllocatorAwareSTL<T>>;
	template<typename T, typename U> using Map = std::map               <T, U, std::less<T>, AllocatorAwareSTL<std::pair<const T, U>>>;
	template<typename T, typename U> using MultiMap = std::multimap     <T, U, std::less<T>, AllocatorAwareSTL<std::pair<const T, U>>>;
	template<typename T, typename U> using HashMap = std::unordered_map <T, U, std::hash<T>, std::equal_to<T>, AllocatorAwareSTL<std::pair<const T, U>>>;
}
