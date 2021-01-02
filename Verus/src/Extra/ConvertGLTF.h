// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Extra
	{
		class ConvertGLTF : public Singleton<ConvertGLTF>, public Object, public BaseConvert
		{
		public:
			ConvertGLTF();
			~ConvertGLTF();
		};
		VERUS_TYPEDEFS(ConvertGLTF);
	}
}
