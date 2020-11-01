// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Extra()
	{
		Extra::FileParser::Make();
	}
	void Free_Extra()
	{
		Extra::FileParser::Free();
	}
}
