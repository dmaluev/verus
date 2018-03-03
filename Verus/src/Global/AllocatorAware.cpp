#include "verus.h"

using namespace verus;

void* AllocatorAware::operator new(size_t size)
{
	VERUS_QREF_UTILS;
	return utils.GetAllocator()->malloc(size);
}

void* AllocatorAware::operator new[](size_t size)
{
	VERUS_QREF_UTILS;
	return utils.GetAllocator()->malloc(size);
}

void AllocatorAware::operator delete(void* p)
{
	VERUS_QREF_UTILS;
	utils.GetAllocator()->free(p);
}

void AllocatorAware::operator delete[](void* p)
{
	VERUS_QREF_UTILS;
	utils.GetAllocator()->free(p);
}

void* AllocatorAware::operator new(size_t size, void* place)
{
	return place;
}

void AllocatorAware::operator delete(void* p, void* place)
{
}

void* AllocatorAware::UtilsMalloc(size_t size)
{
	if (Utils::IsValidSingleton())
	{
		VERUS_QREF_UTILS;
		return utils.GetAllocator()->malloc(size);
	}
	return nullptr;
}

bool AllocatorAware::UtilsFree(void* p)
{
	if (Utils::IsValidSingleton())
	{
		VERUS_QREF_UTILS;
		utils.GetAllocator()->free(p);
		return true;
	}
	return false;
}
