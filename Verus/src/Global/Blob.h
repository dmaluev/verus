// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	struct Blob
	{
		const BYTE* _p;
		const INT64 _size;

		Blob(const BYTE* p = nullptr, const INT64 size = 0) : _p(p), _size(size) {}
	};
	VERUS_TYPEDEFS(Blob);

	struct MutableBlob
	{
		BYTE* _p;
		INT64 _size;

		MutableBlob(BYTE* p = nullptr, const INT64 size = 0) : _p(p), _size(size) {}
	};
	VERUS_TYPEDEFS(MutableBlob);
}
