// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Extra()
	{
		Extra::ConvertGLTF::Make();
		Extra::ConvertX::Make();
	}
	void Free_Extra()
	{
		Extra::ConvertX::Free();
		Extra::ConvertGLTF::Free();
	}
}
