#include "verus.h"

using namespace verus;

Utils::Utils(PBaseAllocator pAlloc)
{
	SetAllocator(pAlloc);
	srand(static_cast<unsigned int>(time(nullptr)));
}

Utils::~Utils()
{
}

void Utils::MakeEx(PBaseAllocator pAlloc)
{
	const float mx = FLT_MAX;
	const UINT32 imx = *(UINT32*)&mx;
	VERUS_RT_ASSERT(imx == 0x7F7FFFFF);

	Utils* p = static_cast<Utils*>(pAlloc->malloc(sizeof(Utils)));
	p = new(p)Utils(pAlloc);
}

void Utils::FreeEx(PBaseAllocator pAlloc)
{
	if (IsValidSingleton())
	{
		Free_Global();
		P()->~Utils();
		pAlloc->free(P());
		Assign(nullptr);
	}
	TestAllocCount();
}

void Utils::ExitSdlLoop()
{
	SDL_Event event = {};
	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);
}

INT32 Utils::Cast32(INT64 x)
{
	if (x < std::numeric_limits<INT32>::min() || x > std::numeric_limits<INT32>::max())
		throw VERUS_RECOVERABLE << "Cast32 failed, x=" << x;
	return static_cast<INT32>(x);
}

UINT32 Utils::Cast32(UINT64 x)
{
	if (x < std::numeric_limits<UINT32>::min() || x > std::numeric_limits<UINT32>::max())
		throw VERUS_RECOVERABLE << "Cast32 failed, x=" << x;
	return static_cast<UINT32>(x);
}
