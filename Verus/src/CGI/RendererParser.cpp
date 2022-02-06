// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

int RendererParser::StrCompare(const void* pA, const void* pB)
{
	CSZ a = static_cast<CSZ>(pA);
	CSZ b = *static_cast<const CSZ*>(pB);
	return strcmp(a, b);
}

int RendererParser::Expect(CSZ& p, CSZ* ppOptions)
{
	int index = 0;
	CSZ option = ppOptions[index];
	while (option)
	{
		const size_t len = strlen(option);
		if (!strncmp(p, option, len))
		{
			p += len;
			return index;
		}
		index++;
		option = ppOptions[index];
	}
	return -1;
}

int RendererParser::ExpectBlendOp(CSZ& p)
{
	static const CSZ blends[] =
	{
		"0",    // = 0
		"1",    // = 1
		"1-da", // = 2
		"1-dc", // = 3
		"1-f",  // = 4
		"1-sa", // = 5
		"1-sc", // = 6
		"da",   // = 7
		"dc",   // = 8
		"f",    // = 9
		"sa",   // = 10
		"sat",  // = 11
		"sc"    // = 12
	};

	const ptrdiff_t len = strchr(p, ')') - p;
	char text[8];
	VERUS_RT_ASSERT(len > 0 && len < VERUS_COUNT_OF(text));
	strncpy(text, p, len);
	text[len] = 0;
	p += len;

	const ptrdiff_t index = static_cast<CSZ*>(bsearch(text, blends, VERUS_COUNT_OF(blends), sizeof(CSZ), StrCompare)) - blends;
	VERUS_RT_ASSERT(index >= 0 && index < VERUS_COUNT_OF(blends));
	return Utils::Cast32(index);
}

int RendererParser::ExpectCompFunc(CSZ& p, char end, bool skipSpace)
{
	static const CSZ cmp[] =
	{
		"0",    // = 0
		"1",    // = 1
		"r!=b", // = 2
		"r<=b", // = 3
		"r<b",  // = 4
		"r==b", // = 5
		"r>=b", // = 6
		"r>b"   // = 7
	};

	if (skipSpace)
		SkipSpace(p);

	const ptrdiff_t len = strchr(p, end) - p;
	char text[8];
	VERUS_RT_ASSERT(len > 0 && len < VERUS_COUNT_OF(text));
	strncpy(text, p, len);
	text[len] = 0;
	p += len;

	if (skipSpace)
		SkipSpace(p);

	const ptrdiff_t index = static_cast<CSZ*>(bsearch(text, cmp, VERUS_COUNT_OF(cmp), sizeof(CSZ), StrCompare)) - cmp;
	VERUS_RT_ASSERT(index >= 0 && index < VERUS_COUNT_OF(cmp));
	return Utils::Cast32(index);
}

int RendererParser::ExpectStencilOp(CSZ& p, char end, bool skipSpace)
{
	static const CSZ ops[] =
	{
		"0",   // = 0
		"b",   // = 1
		"b++", // = 2
		"b--", // = 3
		"bw+", // = 4
		"bw-", // = 5
		"b~",  // = 6
		"r"    // = 7
	};

	if (skipSpace)
		SkipSpace(p);

	const ptrdiff_t len = strchr(p, end) - p;
	char text[8];
	VERUS_RT_ASSERT(len > 0 && len < VERUS_COUNT_OF(text));
	strncpy(text, p, len);
	text[len] = 0;
	p += len;

	if (skipSpace)
		SkipSpace(p);

	const ptrdiff_t index = static_cast<CSZ*>(bsearch(text, ops, VERUS_COUNT_OF(ops), sizeof(CSZ), StrCompare)) - ops;
	VERUS_RT_ASSERT(index >= 0 && index < VERUS_COUNT_OF(ops));
	return Utils::Cast32(index);
}

int RendererParser::SkipSpace(CSZ& p)
{
	CSZ p0 = p;
	while (' ' == *p)
		p++;
	return Utils::Cast32(p - p0);
}
