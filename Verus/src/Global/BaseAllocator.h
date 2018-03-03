#pragma once

namespace verus
{
	class BaseAllocator
	{
	public:
		virtual void* malloc(size_t size) = 0;
		virtual void* realloc(void* memory, size_t size) = 0;
		virtual void free(void* memory) = 0;
	};
	VERUS_TYPEDEFS(BaseAllocator);
}
