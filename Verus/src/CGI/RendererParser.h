// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class RendererParser
		{
			static int StrCompare(const void* pA, const void* pB);

		public:
			static int Expect(CSZ& p, CSZ* ppOptions);
			static int ExpectBlendOp(CSZ& p);
			static int ExpectCompFunc(CSZ& p, char end, bool skipSpace);
			static int ExpectStencilOp(CSZ& p, char end, bool skipSpace);
			static int SkipSpace(CSZ& p);
		};
	}
}
