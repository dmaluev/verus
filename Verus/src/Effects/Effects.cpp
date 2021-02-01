// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Effects()
	{
		Effects::Bloom::Make();
		Effects::Cinema::Make();
		Effects::Ssao::Make();
		Effects::Ssr::Make();
		Effects::Blur::Make();
	}
	void Free_Effects()
	{
		Effects::Blur::Free();
		Effects::Ssr::Free();
		Effects::Ssao::Free();
		Effects::Cinema::Free();
		Effects::Bloom::Free();
	}
}
