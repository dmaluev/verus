// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	// This class can transfer singleton pointers to another library.
	class GlobalVarsClipboard
	{
		struct Pair
		{
			int   _id;
			void* _p;

			Pair(int id, void* p) : _id(id), _p(p) {}
		};

		Vector<Pair> _vPairs;

	public:
		void Copy();
		void Paste();
	};
}
