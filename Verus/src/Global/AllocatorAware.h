// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	// Objects of this class will use the allocator provided by Utils.
	class AllocatorAware
	{
	public:
		void* operator new(size_t size);
		void* operator new[](size_t size);
		void operator delete(void* p);
		void operator delete[](void* p);
		// Placement:
		void* operator new(size_t size, void* place);
		void operator delete(void* p, void* place);

		static void* UtilsMalloc(size_t size);
		static bool UtilsFree(void* p);
	};
}
