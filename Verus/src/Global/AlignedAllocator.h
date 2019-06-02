#pragma once

namespace verus
{
	//! This allocator will reserve aligned memory blocks.
	class AlignedAllocator : public BaseAllocator
	{
		INT64 _memUsedTotal = 0;
		int   _numMalloc = 0;
		int   _numFree = 0;
		int   _mallocDelta = 0;

	public:
		AlignedAllocator() {}

		virtual void* malloc(size_t size) override
		{
			_memUsedTotal += size;
			_numMalloc++;
			_mallocDelta++;
			return _mm_malloc(size, VERUS_MEMORY_ALIGNMENT);
		}

		virtual void* realloc(void* memory, size_t size) override
		{
			void* p = _mm_malloc(size, VERUS_MEMORY_ALIGNMENT);
			memcpy(p, memory, size);
			_mm_free(memory);
			return p;
		}

		virtual void free(void* memory) override
		{
			_numFree++;
			_mallocDelta--;
			_mm_free(memory);
		}
	};
}