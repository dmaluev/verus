#include "verus.h"

namespace verus
{
	void Make_Effects()
	{
		Effects::Bloom::Make();
		Effects::Ssao::Make();
		Effects::Blur::Make();
	}
	void Free_Effects()
	{
		Effects::Blur::Free();
		Effects::Ssao::Free();
		Effects::Bloom::Free();
	}
}
