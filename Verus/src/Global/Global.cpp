#include "verus.h"

int g_numSingletonAlloc;

namespace verus
{
	void Make_Global()
	{
		Timer::Make();
	}
	void Free_Global()
	{
		Timer::Free();
	}
}
