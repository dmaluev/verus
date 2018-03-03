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

		void NextArray(UINT32* p, int num);
		void NextArray(float* p, int num);
		void NextArray(Vector<BYTE>& v);

		std::mt19937& GetGenerator() { return _mt; }

		void Seed(UINT32 seed);
	};
	VERUS_TYPEDEFS(Random);
}
