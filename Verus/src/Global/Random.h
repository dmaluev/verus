// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Random
	{
		std::mt19937 _mt;

	public:
		Random();
		Random(UINT32 seed);
		~Random();

		UINT32 Next();
		double NextDouble();
		float  NextFloat();

		UINT32 Next(UINT32 mn, UINT32 mx);
		double NextDouble(double mn, double mx);
		float  NextFloat(float mn, float mx);

		void NextArray(UINT32* p, int count);
		void NextArray(float* p, int count);
		void NextArray(Vector<BYTE>& v);

		std::mt19937& GetGenerator() { return _mt; }

		void Seed(UINT32 seed);
	};
	VERUS_TYPEDEFS(Random);
}
